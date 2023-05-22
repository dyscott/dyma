#include "dyma_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dyma.h"

static int heap_initialized = 0;

// Calculate the minimum index for a block to be inserted into / retrieved from the free list
int calc_min_free_list_index(size_t size) {
    // Check if size is less than or equal to MIN_BLOCK_SIZE
    if (size <= MIN_BLOCK_SIZE) {
        return 0;
    }
    // Divide by size by MIN_BLOCK_SIZE
    size = (size - 1) / MIN_BLOCK_SIZE;
    // Find the minimum index
    for (int i = 1; i < NUM_FREE_LISTS; i++) {
        if (size <= 1) {
            return i;
        } else {
            size >>= 1;
        }
    }
    return NUM_FREE_LISTS - 1;
}

// Calculate the index for a block to be inserted into / retrieved from the quick list
int calc_quick_list_index(size_t size) {
    int index = (size - MIN_BLOCK_SIZE) / ROW_SIZE;
    // Check if index is out of bounds
    if (index >= NUM_QUICK_LISTS) {
        return -1;
    }
    return index;
}

// Calculate block size for a given payload size
size_t calc_block_size(size_t size) {
    // Calculate necessary block size
    size_t blockSize = size + 8;
    if (blockSize < 32) {
        blockSize = 32;
    } else {
        blockSize = blockSize - 1;
        blockSize = blockSize + (8 - (blockSize % 8));
    }
    return blockSize;
}

// Create a new block starting at *start with size *size
dy_block *create_block(void *start, size_t size) {
    // Create new block
    dy_block *block = (dy_block *)start;
    // Initialize block header
    CLEAR_HEADER(block);
    SET_SIZE(block, size);
    // Initialize block footer
    dy_footer *footer = GET_FOOTER_PTR(block);
    // Clone header into footer
    *footer = (dy_footer)block->header;
    return block;
}

// Insert a block into the free list
void insert_block_free_list(dy_block *block) {
    // Get size of block
    size_t size = GET_SIZE(block);
    // Get index of free list
    int index = calc_min_free_list_index(size);
    // Insert block at head of free list
    dy_block *head = dy_free_list_heads[index].body.links.next;
    block->body.links.next = head;
    block->body.links.prev = &dy_free_list_heads[index];
    head->body.links.prev = block;
    dy_free_list_heads[index].body.links.next = block;
}

// Split a block into two blocks (if possible)
dy_block *split_block(dy_block *block, size_t size) {
    // Get size of block
    size_t blockSize = GET_SIZE(block);
    // Check if block can be split
    if (blockSize - size < MIN_BLOCK_SIZE) {
        return NULL;
    }
    // Create new block
    dy_block *newBlock = create_block((void *)block + size, blockSize - size);
    // Set size of original block
    SET_SIZE(block, size);
    // Set prev_alloc on new block (we would never split a free block)
    SET_PREV_ALLOC(newBlock);
    // Return new block
    return newBlock;
}

// Allocate a block by setting the alloc bit and the prev_alloc bit of the next block
void alloc_block(dy_block *block) {
    // Set alloc bit
    SET_ALLOC(block);
    // Set prev_alloc bit of next block
    dy_block *nextBlock = (void *)block + GET_SIZE(block);
    SET_PREV_ALLOC(nextBlock);
    // If nextBlock is free and not the epilogue, copy header into footer
    if (!GET_ALLOC(nextBlock) && GET_SIZE(nextBlock) != 0) {
        dy_footer *footer = GET_FOOTER_PTR(nextBlock);
        *footer = (dy_footer)nextBlock->header;
    }
}

// Deallocate a block by clearing the alloc bit and the prev_alloc bit of the next block
void dealloc_block(dy_block *block) {
    // Clear alloc bit
    CLEAR_ALLOC(block);
    // Copy header into footer
    dy_footer *footer = GET_FOOTER_PTR(block);
    *footer = (dy_footer)block->header;
    // Clear prev_alloc bit of next block
    dy_block *nextBlock = (void *)block + GET_SIZE(block);
    CLEAR_PREV_ALLOC(nextBlock);
    // Next block cannot be free since we would have coalesced it, so no need to copy header into footer
}

// Coalesce a block with its predecessor
dy_block *coalesce_prev_block(dy_block *block) {
    // Get size of block
    size_t size = GET_SIZE(block);
    // Get size of previous block
    dy_footer *prevFooter = ((void *)block - ROW_SIZE);
    size_t prevSize = *prevFooter & ~0x7;
    // Check if previous block was in free list
    dy_block *prevBlock = (void *)block - prevSize;
    if (prevBlock->body.links.next != NULL && prevBlock->body.links.prev != NULL) {
        // Splice out block from free list
        dy_block *prev = prevBlock->body.links.prev;
        dy_block *next = prevBlock->body.links.next;
        prev->body.links.next = next;
        next->body.links.prev = prev;
        // Set next and prev to NULL
        prevBlock->body.links.next = NULL;
        prevBlock->body.links.prev = NULL;
    }
    // Check if previous block had prev_alloc bit set
    size_t prevAlloc = GET_PREV_ALLOC(prevBlock);
    // Create new block
    dy_block *newBlock = create_block((void *)block - prevSize, size + prevSize);
    // Set prev_alloc bit of new block
    if (prevAlloc) {
        SET_PREV_ALLOC(newBlock);
    }
    // Return new block
    return newBlock;
}

// Coalesce a block with its successor
dy_block *coalesce_next_block(dy_block *block) {
    // Get size of block
    size_t size = GET_SIZE(block);
    // Get size of next block
    dy_block *nextBlock = (void *)block + size;
    size_t nextSize = GET_SIZE(nextBlock);
    // Check if next block was in free list
    if (nextBlock->body.links.next != NULL && nextBlock->body.links.prev != NULL) {
        // Splice out block from free list
        dy_block *prev = nextBlock->body.links.prev;
        dy_block *next = nextBlock->body.links.next;
        prev->body.links.next = next;
        next->body.links.prev = prev;
        // Set next and prev to NULL
        nextBlock->body.links.next = NULL;
        nextBlock->body.links.prev = NULL;
    }
    // Check if original block had prev_alloc bit set
    size_t prevAlloc = GET_PREV_ALLOC(block);
    // Create new block
    dy_block *newBlock = create_block((void *)block, size + nextSize);
    // Set prev_alloc bit of new block
    if (prevAlloc) {
        SET_PREV_ALLOC(newBlock);
    }
    // Return new block
    return newBlock;
}

// Flush a quick list
void flush_quick_list(int index) {
    // Get head of quick list
    dy_block *head = dy_quick_lists[index].first;
    // Iterate through quick list
    while ((void *)head != &dy_quick_lists[index] && head != NULL) {
        // Get next block
        dy_block *next = head->body.links.next;
        // Check if previous block is free and coalesce
        if (!GET_PREV_ALLOC(head)) {
            head = coalesce_prev_block(head);
        }
        // Check if next block is free and coalesce
        if (!GET_ALLOC((dy_block *)((void *)head + GET_SIZE(head)))) {
            head = coalesce_next_block(head);
        }
        // Deallocate block
        dealloc_block(head);
        // Insert block into free list
        insert_block_free_list(head);
        // Set head to next
        head = next;
    }
    // Set length to 0 and first to NULL
    dy_quick_lists[index].length = 0;
    dy_quick_lists[index].first = NULL;
}

/**
 * Initialize the heap and associated data structures.
 * @return 0 on success, -1 on failure.
 */
int init_heap() {
    // Check if heap is already initialized
    if (heap_initialized) {
        return 0;
    }

    // Get page of memory
    void *page = dy_mem_grow();
    if (page == NULL) {
        // If page is NULL, no memory could be allocated
        dy_errno = ENOMEM;
        return -1;
    }
    void *pageEnd = dy_mem_end();

    // Create prologue block
    dy_block *prologue = page;

    // Initialize prologue header
    CLEAR_HEADER(prologue);
    SET_ALLOC(prologue);
    SET_SIZE(prologue, MIN_BLOCK_SIZE);

    // Set payload to all 0s
    memset(prologue->body.payload, 0, MIN_BLOCK_SIZE - ROW_SIZE);

    // Create epilogue block
    dy_block *epilogue = (void *)pageEnd - ROW_SIZE;

    // Initialize epilogue header
    CLEAR_HEADER(epilogue);
    SET_ALLOC(epilogue);
    SET_SIZE(epilogue, 0);

    // Initialize free lists
    for (int i = 0; i < NUM_FREE_LISTS; i++) {
        dy_free_list_heads[i].body.links.next = &dy_free_list_heads[i];
        dy_free_list_heads[i].body.links.prev = &dy_free_list_heads[i];
    }

    // Initialize quick lists
    for (int i = 0; i < NUM_QUICK_LISTS; i++) {
        dy_quick_lists[i].length = 0;
        dy_quick_lists[i].first = NULL;
    }

    // Create block from remaining memory
    size_t size = (size_t)(pageEnd - page) - MIN_BLOCK_SIZE - ROW_SIZE;
    dy_block *free = create_block(page + MIN_BLOCK_SIZE, size);
    
    // Previous block should be set to allocated
    SET_PREV_ALLOC(free);

    // Insert first free block into free list
    insert_block_free_list(free);

    heap_initialized = 1;
    return 0;
}

/**
 * Get a block from the quick list, if possible.
 * @param block_size The size of the block to get.
 * @return A pointer to the block, or NULL if no block was found.
 */
dy_block *get_quick_list_block(size_t block_size) {
    // Get index of quick list
    int index = calc_quick_list_index(block_size);

    // If index is out of bounds, return NULL
    if (index == -1) {
        return NULL;
    }

    // Check if quick list is empty
    if (dy_quick_lists[index].length == 0) {
        return NULL;
    }

    // Get first block in quick list
    dy_block *block = dy_quick_lists[index].first;

    // Remove block from quick list
    dy_quick_lists[index].first = block->body.links.next;
    dy_quick_lists[index].length--;

    // Clear quick list bit
    CLEAR_IN_QUICK_LIST(block);

    // Allocate block
    alloc_block(block);

    // Return block
    return block;
}

/**
 * Get a block from the free list, if possible.
 * @param block_size The minimum size of the block to get.
 * @return A pointer to the block, or NULL if no block was found.
 */
dy_block *get_free_list_block(size_t block_size) {
    // Get the minimum index for the free list
    int index = calc_min_free_list_index(block_size);

    // Iterate through free lists
    for (int i = index; i < NUM_FREE_LISTS; i++) {
        // Check if free list is empty (sentinel node points to itself)
        if (dy_free_list_heads[i].body.links.next == &dy_free_list_heads[i]) {
            continue;
        }

        // Get first block in free list
        dy_block *block = dy_free_list_heads[i].body.links.next;

        // Check if block is large enough
        while (block != &dy_free_list_heads[i] && GET_SIZE(block) < block_size) {
            block = block->body.links.next;
        }
        if (block == &dy_free_list_heads[i]) {
            continue;
        }

        // Splice out block from free list
        dy_block *next = block->body.links.next;
        dy_block *prev = block->body.links.prev;
        next->body.links.prev = prev;
        prev->body.links.next = next;

        // Split block if possible
        dy_block *split = split_block(block, block_size);
        if (split != NULL) {
            // Insert split block into free list
            insert_block_free_list(split);
        }

        // Allocate block
        alloc_block(block);

        // Return block
        return block;
    }

    // If no block was found, return NULL
    return NULL;
}

/**
 * Get a block from the heap, if possible.
 * @param block_size The minimum size of the block to get.
 * @return A pointer to the block, or NULL if no block was found.
 */
dy_block *get_heap_block(size_t block_size) {
    dy_block *block = NULL;
    do {
        // Get new page of memory
        void *page = dy_mem_grow();
        if (page == NULL) {
            // If page is NULL, no memory could be allocated
            // If the current block is large enough, at least add it to the free list
            if (block != NULL) {
                // Copy header into footer
                dy_footer *footer = GET_FOOTER_PTR(block);
                *footer = (dy_footer)block->header;
                insert_block_free_list(block);
            }
            dy_errno = ENOMEM;
            return NULL;
        }
        void *pageEnd = dy_mem_end();

        // Get whether the previous block was allocated
        dy_block *epilogue = page - ROW_SIZE;
        bool prevAlloc = GET_PREV_ALLOC(epilogue);

        // Create new epilogue
        dy_block *newEpilogue = pageEnd - ROW_SIZE;
        CLEAR_HEADER(newEpilogue);
        SET_ALLOC(newEpilogue);
        SET_SIZE(newEpilogue, 0);

        // Create new block from remaining memory
        size_t size = (size_t)(pageEnd - page);
        block = create_block(epilogue, size);

        // If the previous block was free, coalesce with the new block
        if (!prevAlloc) {
            block = coalesce_prev_block(block);
        } else {
            // Set previous block as allocated
            SET_PREV_ALLOC(block);
        }
    } while (GET_SIZE(block) < block_size);

    // Split block if possible
    dy_block *split = split_block(block, block_size);
    if (split != NULL) {
        // Insert split block into free list
        insert_block_free_list(split);
    }

    // Allocate block
    alloc_block(block);

    // Return block
    return block;
}

/**
 * Check if a pointer for dy_free is valid.
 * @param pp The pointer to check.
 * @return 0 if the pointer is valid, -1 otherwise.
 */
int check_pointer(void* pp) {
    // Check if pointer is NULL
    if (pp == NULL) {
        return -1;
    }

    // Check if pointer is aligned (8 bytes)
    if ((size_t)pp % 8 != 0) {
        return -1;
    }

    // Check if start of block is in heap
    dy_block *block = pp - ROW_SIZE;
    if ((void *)block < dy_mem_start() || (void *)block > dy_mem_end()) {
        return -1;
    }

    // Check if block size is valid
    size_t size = GET_SIZE(block);
    if (size < MIN_BLOCK_SIZE || size % 8 != 0) {
        return -1;
    }

    // Check if end of block is in heap
    dy_footer *end = (dy_footer *)((void *)block + size);
    if ((void *)end < dy_mem_start() || (void *)end > dy_mem_end()) {
        return -1;
    }

    // Check if block is free or in quick list
    if (!GET_ALLOC(block) || GET_IN_QUICK_LIST(block)) {
        return -1;
    }
    
    // Check alloc bit of previous block if prev_alloc is 0
    if (!GET_PREV_ALLOC(block)) {
        dy_footer *prev = (dy_footer *)((void *)block - ROW_SIZE);
        if ((size_t) *prev & 0x1) {
            return -1;
        }
    }
    return 0;
}

/**
 * Free a block to a quick list, if possible.
 * @param block The block to free.
 * @return 0 if the block was freed to a quick list, -1 otherwise.
 */
int free_to_quick_list(dy_block *block) {
    // Get index of quick list
    int index = calc_quick_list_index(GET_SIZE(block));

    // If index is out of bounds, return -1
    if (index == -1) {
        return -1;
    }

    // Check if quick list is full, flush if so
    if (dy_quick_lists[index].length == QUICK_LIST_MAX) {
        flush_quick_list(index);
    }

    // Add block to quick list
    block->body.links.next = dy_quick_lists[index].first;
    dy_quick_lists[index].first = block;
    dy_quick_lists[index].length++;

    // Set quick list bit
    SET_IN_QUICK_LIST(block);

    // Return 0
    return 0;
}

/**
 * Free a block to the free list.
 * @param block The block to free.
 */
void free_to_free_list(dy_block *block) {
    // Check if block can be coalesced with previous block
    if (!GET_PREV_ALLOC(block)) {
        block = coalesce_prev_block(block);
    }

    // Check if block can be coalesced with next block
    if (!GET_ALLOC((dy_block *)((void *)block + GET_SIZE(block)))) {
        block = coalesce_next_block(block);
    }

    // Deallocate block
    dealloc_block(block);
    
    // Insert block into free list
    insert_block_free_list(block);
}