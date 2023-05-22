#include "dyma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "dyma_utils.h"

/**
 * Allocates an uninitialized block of memory of a specified size in bytes.
 * @param size Size of memory to allocate in bytes.
 * @return If successful, a pointer to an uninitialized region of memory of the specified size
 *         If size is 0, then NULL is returned.
 *         If allocation fails, then NULL is returned and dy_errno is set to ENOMEM.
 */
void *dy_malloc(size_t size) {
    // Request size check
    if (size == 0) {
        return NULL;
    }

    // Initialize heap (if not already initialized)
    int result = init_heap();
    if (result) {
        return NULL;
    }

    // Calculate necessary block size
    size_t blockSize = calc_block_size(size);

    // Check quick lists
    dy_block *block = get_quick_list_block(blockSize);
    if (block != NULL) {
        // Return pointer to payload
        return block->body.payload;
    }

    // Check free lists
    block = get_free_list_block(blockSize);
    if (block != NULL) {
        // Return pointer to payload
        return block->body.payload;
    }

    // Finally, get a new block from the heap
    block = get_heap_block(blockSize);
    if (block != NULL) {
        // Return pointer to payload
        return block->body.payload;
    }
    return NULL;
}

/**
 * Frees a previously block of allocated memory, allowing it to be reused.
 * @param ptr Pointer to block of memory.
 *
 * If ptr is invalid, abort() will be called to exit the program.
 */
void dy_free(void *pp) {
    // Pointer check
    if (check_pointer(pp)) {
        abort();
    }

    // Attempt to add block to quick list
    dy_block *block = (dy_block *)((void *)pp - ROW_SIZE);
    int result = free_to_quick_list(block);
    if (result == 0) {
        return;
    }

    // Attempt to add block to free list
    free_to_free_list(block);
}

/**
 * Reallocates a previously allocated block of memory, changing its size to the specified size.
 *
 * @param ptr Address of the memory block to be reallocated.
 * @param size The new size for the memory block, in bytes.
 *
 * @return If successful, a pointer to the new memory block is returned, which may be the same as ptr.
 *         If an invalid pointer is provided, then NULL is returned and dy_errno is set to EINVAL.
 *         If there is no memory available, then NULL is returned and dy_errno is set to ENOMEM.
 */
void *dy_realloc(void *pp, size_t rsize) {
    // Pointer check
    if(check_pointer(pp)) {
        dy_errno = EINVAL;
        return NULL;
    }

    // Zero size check
    if (rsize == 0) {
        dy_free(pp);
        return NULL;
    }

    // Check the size of the current block
    dy_block *block = ((void *)pp - ROW_SIZE);
    size_t blockSize = calc_block_size(rsize);

    // Handle growing
    if (blockSize > GET_SIZE(block)) {
        // Get new block
        void *newPtr = dy_malloc(rsize);
        if (newPtr == NULL) {
            return NULL;
        }

        // Copy data from old block to new block
        memcpy(newPtr, pp, GET_SIZE(block) - ROW_SIZE);

        // Free old block
        dy_free(pp);

        // Return pointer to new block
        return newPtr;
    }

    // Handle shrinking
    if (blockSize < GET_SIZE(block)) {
        // Split block
        dy_block *newBlock = split_block(block, blockSize);

        // If new block was created, add it to the free list
        if (newBlock != NULL) {
            free_to_free_list(newBlock);
        }

        // Return pointer to old block
        return pp;
    }

    // Handle same size
    return pp;
}

/**
 * Allocates a block of memory with a specified alignment.
 *
 * @param align The alignment required for the returned pointer.
 * @param size Size of memory to allocate in bytes.
 *
 * @return If successful, a pointer to an uninitialized region of memory of the specified size and alignment.
 *         If size is 0, then NULL is returned.
 *         If align is not a power of two or is less than the minimum block size, then NULL is returned and dy_errno is set to EINVAL.
 *         If the allocation is not successful, then NULL is returned and dy_errno is set to ENOMEM.
 */
void *dy_memalign(size_t size, size_t align) {
    // Alignment size check
    if (align < ROW_SIZE || align & (align - 1)) {
        dy_errno = EINVAL;
        return NULL;
    }

    // Request size check
    if (size == 0) {
        return NULL;
    }

    // Allocate a block of size + align + min block size + header + footer
    void *ptr = dy_malloc(size + align + MIN_BLOCK_SIZE + 8); // malloc already adds one +8
    if (ptr == NULL) {
        return NULL;
    }

    // Check if ptr is already aligned
    if ((uintptr_t)ptr % align == 0) {
        // Free the additional space
        dy_block *block = (dy_block *)((void *)ptr - ROW_SIZE);
        size_t blockSize = calc_block_size(size);
        dy_block *splitBlock = split_block(block, blockSize);
        if (splitBlock != NULL) {
            // Free the new block
            free_to_free_list(splitBlock);
        }
        return ptr;
    }

    // Get the block
    dy_block *block = (dy_block *)((void *)ptr - ROW_SIZE);

    // Find the first aligned address after the minimum block size
    void *start = (void *)block + MIN_BLOCK_SIZE + ROW_SIZE;
    uintptr_t diff = (uintptr_t)start % align;
    void *aligned = start + (align - diff);

    // Split the block
    dy_block *newBlock = split_block(block, (uintptr_t)aligned - (uintptr_t)block - ROW_SIZE);

    // Mark the new block as allocated
    alloc_block(newBlock);

    // Free the old block
    free_to_free_list(block);

    // Attempt to split the new block
    size_t blockSize = calc_block_size(size);
    dy_block *newNewBlock = split_block(newBlock, blockSize);
    if (newNewBlock != NULL) {
        // Free the new block
        free_to_free_list(newNewBlock);
    }

    // Return pointer to aligned block
    return aligned;
}
