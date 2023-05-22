#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>

#include "dyma.h"
#include "dyma_utils.h"
#define TEST_TIMEOUT 15

void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for (int i = 0; i < NUM_FREE_LISTS; i++) {
        dy_block *bp = dy_free_list_heads[i].body.links.next;
        while (bp != &dy_free_list_heads[i]) {
            if (size == 0 || size == (bp->header & ~0x7))
                cnt++;
            bp = bp->body.links.next;
        }
    }
    if (size == 0) {
        cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
                     count, cnt);
    } else {
        cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
                     size, count, cnt);
    }
}

void assert_free_list_size(int index, int size) {
    int cnt = 0;
    dy_block *bp = dy_free_list_heads[index].body.links.next;
    while (bp != &dy_free_list_heads[index]) {
        cnt++;
        bp = bp->body.links.next;
    }
    cr_assert_eq(cnt, size, "Free list %d has wrong number of free blocks (exp=%d, found=%d)",
                 index, size, cnt);
}

void assert_quick_list_block_count(size_t size, int count) {
    int cnt = 0;
    for (int i = 0; i < NUM_QUICK_LISTS; i++) {
        dy_block *bp = dy_quick_lists[i].first;
        while (bp != NULL) {
            if (size == 0 || size == (bp->header & ~0x7))
                cnt++;
            bp = bp->body.links.next;
        }
    }
    if (size == 0) {
        cr_assert_eq(cnt, count, "Wrong number of quick list blocks (exp=%d, found=%d)",
                     count, cnt);
    } else {
        cr_assert_eq(cnt, count, "Wrong number of quick list blocks of size %ld (exp=%d, found=%d)",
                     size, count, cnt);
    }
}

Test(dyma_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
    dy_errno = 0;
    size_t sz = sizeof(int);
    int *x = dy_malloc(sz);

    cr_assert_not_null(x, "x is NULL!");

    *x = 4;

    cr_assert(*x == 4, "dy_malloc failed to give proper space for an int!");

    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 1);
    assert_free_block_count(4024, 1);
    assert_free_list_size(7, 1);

    cr_assert(dy_errno == 0, "dy_errno is not zero!");
    cr_assert(dy_mem_start() + PAGE_SZ == dy_mem_end(), "Allocated more than necessary!");
}

Test(dyma_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
    dy_errno = 0;

    void *x = dy_malloc(16336);
    cr_assert_not_null(x, "x is NULL!");
    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 0);
    cr_assert(dy_errno == 0, "dy_errno is not 0!");
}

Test(dyma_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
    dy_errno = 0;
    void *x = dy_malloc(PAGE_SZ * 1024);

    cr_assert_null(x, "x is not NULL!");
    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 1);
    assert_free_block_count(4194264, 1);
    cr_assert(dy_errno == ENOMEM, "dy_errno is not ENOMEM!");
}

Test(dyma_suite, free_quick, .timeout = TEST_TIMEOUT) {
    dy_errno = 0;
    size_t sz_x = 8, sz_y = 32, sz_z = 1;
    dy_malloc(sz_x);
    void *y = dy_malloc(sz_y);
    dy_malloc(sz_z);

    dy_free(y);

    assert_quick_list_block_count(0, 1);
    assert_quick_list_block_count(40, 1);
    assert_free_block_count(0, 1);
    assert_free_block_count(3952, 1);
    cr_assert(dy_errno == 0, "dy_errno is not zero!");
}

Test(dyma_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
    dy_errno = 0;
    size_t sz_x = 8, sz_y = 200, sz_z = 1;
    dy_malloc(sz_x);
    void *y = dy_malloc(sz_y);
    dy_malloc(sz_z);

    dy_free(y);

    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 2);
    assert_free_block_count(208, 1);
    assert_free_block_count(3784, 1);

    cr_assert(dy_errno == 0, "dy_errno is not zero!");
}

Test(dyma_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
    dy_errno = 0;
    size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
    dy_malloc(sz_w);
    void *x = dy_malloc(sz_x);
    void *y = dy_malloc(sz_y);
    dy_malloc(sz_z);

    dy_free(y);
    dy_free(x);

    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 2);
    assert_free_block_count(520, 1);
    assert_free_block_count(3472, 1);

    cr_assert(dy_errno == 0, "dy_errno is not zero!");
}

Test(dyma_suite, freelist, .timeout = TEST_TIMEOUT) {
    size_t sz_u = 200, sz_v = 300, sz_w = 200, sz_x = 500, sz_y = 200, sz_z = 700;
    void *u = dy_malloc(sz_u);
    dy_malloc(sz_v);
    void *w = dy_malloc(sz_w);
    dy_malloc(sz_x);
    void *y = dy_malloc(sz_y);
    dy_malloc(sz_z);

    dy_free(u);
    dy_free(w);
    dy_free(y);

    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 4);
    assert_free_block_count(208, 3);
    assert_free_block_count(1896, 1);
    assert_free_list_size(3, 3);
    assert_free_list_size(6, 1);
}

Test(dyma_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
    size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
    void *x = dy_malloc(sz_x);
    dy_malloc(sz_y);
    x = dy_realloc(x, sz_x1);

    cr_assert_not_null(x, "x is NULL!");
    dy_block *bp = (dy_block *)((char *)x - sizeof(dy_header));
    cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert((bp->header & ~0x7) == 88, "Realloc'ed block size not what was expected!");

    assert_quick_list_block_count(0, 1);
    assert_quick_list_block_count(32, 1);
    assert_free_block_count(0, 1);
    assert_free_block_count(3904, 1);
}

Test(dyma_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
    size_t sz_x = sizeof(int) * 20, sz_y = sizeof(int) * 16;
    void *x = dy_malloc(sz_x);
    void *y = dy_realloc(x, sz_y);

    cr_assert_not_null(y, "y is NULL!");
    cr_assert(x == y, "Payload addresses are different!");

    dy_block *bp = (dy_block *)((char *)y - sizeof(dy_header));
    cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert((bp->header & ~0x7) == 88, "Realloc'ed block size not what was expected!");

    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 1);
    assert_free_block_count(3968, 1);
}

Test(dyma_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
    size_t sz_x = sizeof(double) * 8, sz_y = sizeof(int);
    void *x = dy_malloc(sz_x);
    void *y = dy_realloc(x, sz_y);

    cr_assert_not_null(y, "y is NULL!");

    dy_block *bp = (dy_block *)((char *)y - sizeof(dy_header));
    cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert((bp->header & ~0x7) == 32, "Realloc'ed block size not what was expected!");

    assert_quick_list_block_count(0, 0);
    assert_free_block_count(0, 1);
    assert_free_block_count(4024, 1);
}

Test(dyma_suite, calc_min_free_list, .timeout = TEST_TIMEOUT) {
    /**
     * Testing "calc_min_free_list_index" helper to ensure it returns the correct min
     * index for a given size in the free list.
     */
    int M = MIN_BLOCK_SIZE;
    // Test 1: size = M
    cr_assert(calc_min_free_list_index(M) == 0, "calc_min_index(M) != 0");
    // Test 2: size = M + 1
    cr_assert(calc_min_free_list_index(M + 1) == 1, "calc_min_index(M + 1) != 1");
    // Test 3: size = 2M
    cr_assert(calc_min_free_list_index(2 * M) == 1, "calc_min_index(2 * M) != 1");
    // Test 4: size = 2M + 1
    cr_assert(calc_min_free_list_index(2 * M + 1) == 2, "calc_min_index(2 * M + 1) != 2");
    // Test 5: size = 4M
    cr_assert(calc_min_free_list_index(4 * M) == 2, "calc_min_index(4 * M) != 2");
    // Test 6: size = 4M + 1
    cr_assert(calc_min_free_list_index(4 * M + 1) == 3, "calc_min_index(4 * M + 1) != 3");
    // Test 7: size = 8M
    cr_assert(calc_min_free_list_index(8 * M) == 3, "calc_min_index(8 * M) != 3");
    // Test 8: size = 8M + 1
    cr_assert(calc_min_free_list_index(8 * M + 1) == 4, "calc_min_index(8 * M + 1) != 4");
    // Test 9: size = 16M
    cr_assert(calc_min_free_list_index(16 * M) == 4, "calc_min_index(16 * M) != 4");
    // Test 10: size = 16M + 1
    cr_assert(calc_min_free_list_index(16 * M + 1) == 5, "calc_min_index(16 * M + 1) != 5");
    // Test 11: size = 32M
    cr_assert(calc_min_free_list_index(32 * M) == 5, "calc_min_index(32 * M) != 5");
    // Test 12: size = 32M + 1
    cr_assert(calc_min_free_list_index(32 * M + 1) == 6, "calc_min_index(32 * M + 1) != 6");
    // Test 13: size = 64M
    cr_assert(calc_min_free_list_index(64 * M) == 6, "calc_min_index(64 * M) != 6");
    // Test 14: size = 64M + 1
    cr_assert(calc_min_free_list_index(64 * M + 1) == 7, "calc_min_index(64 * M + 1) != 7");
    // Test 15: size = 128M
    cr_assert(calc_min_free_list_index(128 * M) == 7, "calc_min_index(128 * M) != 7");
    // Test 16: size = 128M + 1
    cr_assert(calc_min_free_list_index(128 * M + 1) == 8, "calc_min_index(128 * M + 1) != 8");
    // Test 17: size = 256M
    cr_assert(calc_min_free_list_index(256 * M) == 8, "calc_min_index(256 * M) != 8");
    // Test 18: size = 256M + 1
    cr_assert(calc_min_free_list_index(256 * M + 1) == 9, "calc_min_index(256 * M + 1) != 9");
    // Test 19: size = 512M
    cr_assert(calc_min_free_list_index(512 * M) == 9, "calc_min_index(512 * M) != 9");
    // Test 20: size = 1024M
    cr_assert(calc_min_free_list_index(1024 * M) == 9, "calc_min_index(1024 * M) != 19");
}

Test(dyma_suite, calc_block_size, .timeout = TEST_TIMEOUT) {
    /**
     * Testing "calc_block_size" helper to ensure it returns the correct
     * block size for a given payload size.
     */

    // Test 1: size = 1
    cr_assert(calc_block_size(1) == 32, "calc_block_size(1) != 32");
    // Test 2: size = 24
    cr_assert(calc_block_size(24) == 32, "calc_block_size(24) != 32");
    // Test 3: size = 25
    cr_assert(calc_block_size(25) == 40, "calc_block_size(25) != 40");
    // Test 4: size = 48
    cr_assert(calc_block_size(48) == 56, "calc_block_size(48) != 56");
    // Test 5: size = 49
    cr_assert(calc_block_size(49) == 64, "calc_block_size(49) != 64");
    // Test 6: size = 56
    cr_assert(calc_block_size(56) == 64, "calc_block_size(56) != 64");
    // Test 7: size = 57
    cr_assert(calc_block_size(57) == 72, "calc_block_size(57) != 72");
    // Test 8: size = 100
    cr_assert(calc_block_size(100) == 112, "calc_block_size(100) != 112");
    // Test 9: size = 1000
    cr_assert(calc_block_size(1000) == 1008, "calc_block_size(1000) != 1008");
    // Test 10: size = 10000
    cr_assert(calc_block_size(10000) == 10008, "calc_block_size(10000) != 10008");
}

Test(dyma_suite, calc_quick_list, .timeout = TEST_TIMEOUT) {
    /**
     * Testing "calc_quick_list_index" helper to ensure it returns the quick list
     * index for a given payload size.
     */
    // Test index 0 - 19
    for (int i = 0; i < 20; i++) {
        int size = 32 + i * 8;
        cr_assert(calc_quick_list_index(size) == i, "calc_quick_list_index(%d) != %d", size, i);
    }

    // Test too large
    int size = 32 * 20 + 8;
    cr_assert(calc_quick_list_index(size) == -1, "calc_quick_list_index(%d) != -1", size);
}

Test(dyma_suite, malloc_size_zero, .timeout = TEST_TIMEOUT) {
    /**
     * Test passing a size of 0 to dy_malloc.
     */
    dy_errno = 0;
    void *ptr = dy_malloc(0);

    cr_assert(ptr == NULL, "dy_malloc(0) != NULL");
}

Test(dyma_suite, malloc_enomem, .timeout = TEST_TIMEOUT) {
    /**
     * Test calling dy_malloc after we already took all the memory.
     */
    dy_errno = 0;
    while (dy_mem_grow() != NULL) {
        // Grow the heap
    }
    void *ptr = dy_malloc(1);

    cr_assert(ptr == NULL, "dy_malloc(1) != NULL");
    cr_assert(dy_errno == ENOMEM, "dy_errno != ENOMEM");
}

Test(dyma_suite, flush_quicklist, .timeout = TEST_TIMEOUT) {
    /**
     * Test flushing the quick list.
     */
    dy_errno = 0;
    size_t size = 24;

    // Fill the quick list for size 32
    void *ptrs[QUICK_LIST_MAX + 1];
	for (int i = 0; i <= QUICK_LIST_MAX; i++) {
		ptrs[i] = dy_malloc(size);
		cr_assert(ptrs[i] != NULL, "dy_malloc(%d) == NULL", size);
	}
	for (int i = 0; i < QUICK_LIST_MAX; i++) {
		dy_free(ptrs[i]);
	}

    // Quick list for 32 should be full, free list should have 1 block
	assert_quick_list_block_count(32, QUICK_LIST_MAX);
	assert_free_block_count(0, 1);

	// Flush the quick list by freeing the last block
	dy_free(ptrs[QUICK_LIST_MAX]);

	// Quick list for 32 should contain 1 block, free list should have 2 blocks (fragmented by the one element in the quick list so sad)
	assert_quick_list_block_count(32, 1);
	assert_free_block_count(0, 2);

	// Get a block from the quick list
	void *ptr = dy_malloc(size);
	cr_assert(ptr != NULL, "dy_malloc(%d) == NULL", size);

	// Quick list for 32 should empty, free list should have 2 blocks
	assert_quick_list_block_count(32, 0);
	assert_free_block_count(0, 2);
}

Test(dyma_suite, check_invalid_pointer, .timeout = TEST_TIMEOUT) {
	/**
	 * Test freeing various invalid pointers.
	 */

	// Test 1: Free a null pointer
	int valid = check_pointer(NULL);
	cr_assert(valid == -1, "check_pointer(NULL) != -1");

	// Test 2: Free a pointer that is not aligned
	void *ptr = dy_malloc(sizeof(int) * 64);
	valid = check_pointer(ptr + 1);
	cr_assert(valid == -1, "check_pointer(ptr + 1) != -1");

	// Test 3: Free a pointer that has a block size less than 32
	dy_block *block = ptr - ROW_SIZE;
	size_t orig = GET_SIZE(block);
	SET_SIZE(block, 16);
	valid = check_pointer(ptr);
	cr_assert(valid == -1, "check_pointer(ptr) != -1");

	// Test 4: Free a pointer that is not in the heap
	int *ptr2 = malloc(sizeof(int));
	valid = check_pointer(ptr2);
	cr_assert(valid == -1, "check_pointer(ptr2) != -1");

	// Test 5: Free a pointer that has its end past the end of the heap
	SET_SIZE(block, 100000);
	valid = check_pointer(ptr);
	cr_assert(valid == -1, "check_pointer(ptr) != -1");

	// Test 6: Free a pointer that has already been freed
	SET_SIZE(block, orig);
	dy_free(ptr);
	valid = check_pointer(ptr);
	cr_assert(valid == -1, "check_pointer(ptr) != -1");

    // Test 7: Free a pointer that has prev_alloc set to 0 but the previous block is allocated
    ptr = dy_malloc(sizeof(int) * 64);
    ptr2 = dy_malloc(sizeof(int) * 32);
    dy_free(ptr);
    // Set the previous block to allocated
    size_t *footer = (void *)ptr2 - 2 * ROW_SIZE;
    *footer = *(footer) | THIS_BLOCK_ALLOCATED;
    // Check that the pointer is invalid
    valid = check_pointer(ptr2);
    cr_assert(valid == -1, "check_pointer(ptr2) != -1");
}

Test(dyma_suite, malloc_gt_page, .timeout = TEST_TIMEOUT) {
	/**
	 * Test allocating more than a page.
	 */
	dy_errno = 0;
	size_t size_x = PAGE_SZ - MIN_BLOCK_SIZE - 2 * ROW_SIZE;

	// Allocate the entire first page
	void *ptr = dy_malloc(size_x);
	cr_assert(ptr != NULL, "dy_malloc(%d) == NULL", size_x);
    assert_free_block_count(0, 0);

	// Allocate about half of the second page
	size_t size_y = PAGE_SZ / 2;
	void *ptr2 = dy_malloc(size_y);
	cr_assert(ptr2 != NULL, "dy_malloc(%d) == NULL", size_y);
    assert_free_block_count(0, 1);

	// Free the first block
	dy_free(ptr);
	assert_free_block_count(0, 2);

	// Free the second block
	dy_free(ptr2);
	assert_free_block_count(0, 1);
}

Test(dyma_suite, malloc_gt_page2, .timeout = TEST_TIMEOUT) {
	/**
	 * Test allocating more than a page, similar to test 8 but with slightly different order.
	 */
	dy_errno = 0;
	size_t size_x = PAGE_SZ - MIN_BLOCK_SIZE - 2 * ROW_SIZE;

	// Allocate the entire first page
	void *ptr = dy_malloc(size_x);
	cr_assert(ptr != NULL, "dy_malloc(%d) == NULL", size_x);
    assert_free_block_count(0, 0);

	// Allocate about half of the second page
	size_t size_y = PAGE_SZ / 2;
	void *ptr2 = dy_malloc(size_y);
	cr_assert(ptr2 != NULL, "dy_malloc(%d) == NULL", size_y);
    assert_free_block_count(0, 1);

    // Allocate an additional
    void *ptr3 = dy_malloc(1024);
    cr_assert(ptr3 != NULL, "dy_malloc(1) == NULL");
    assert_free_block_count(0, 1);

	// Free the second block
	dy_free(ptr2);
	assert_free_block_count(0, 2);

	// Free the first block
	dy_free(ptr);
	assert_free_block_count(0, 2);

    // Free the third block
    dy_free(ptr3);
    assert_free_block_count(0, 1);
}

Test(dyma_suite, realloc_edge_cases, .timeout = TEST_TIMEOUT) {
	/**
	 * Test edge cases for dy_realloc.
     */
    dy_errno = 0;

    // Test 1: Realloc to size 0
    void *ptr = dy_malloc(1024);
    cr_assert(ptr != NULL, "dy_malloc(1024) == NULL");
    ptr = dy_realloc(ptr, 0);
    cr_assert(ptr == NULL, "dy_realloc(ptr, 0) != NULL");

    // Test 2: Realloc to the same size
    ptr = dy_malloc(1024);
    void *old_ptr = ptr;
    cr_assert(ptr != NULL, "dy_malloc(1024) == NULL");
    ptr = dy_realloc(ptr, 1024);
    cr_assert(ptr == old_ptr, "dy_realloc(ptr, 1024) != ptr");

    // Test 3: Realloc to too large of a size
    ptr = dy_malloc(1024);
    cr_assert(ptr != NULL, "dy_malloc(1024) == NULL");
    ptr = dy_realloc(ptr, PAGE_SZ * 1024);
    cr_assert(ptr == NULL, "dy_realloc(ptr, PAGE_SZ * 20) != NULL");
    cr_assert(dy_errno == ENOMEM, "dy_errno != ENOMEM");
}

Test(dyma_suite, memalign_1024, .timeout = TEST_TIMEOUT) {
	/**
	 * Test allocating a 1024 bit-aligned block.
     */
    size_t sz_x = 1024;
    size_t align = 1024;
    void *x = dy_memalign(sz_x, align);
    assert_free_block_count(0, 2);

    // Check that the pointer is aligned
    cr_assert((uintptr_t)x % align == 0, "x is not aligned");

    // Free the block
    dy_free(x);
    assert_free_block_count(0, 1);
}

Test(dyma_suite, memalign_8, .timeout = TEST_TIMEOUT) {
	/**
	 * Test allocating a 8 bit-aligned block.
     */
    size_t sz_x = 1024;
    size_t align = 8;
    void *x = dy_memalign(sz_x, align);
    assert_free_block_count(0, 1);

    // Check that the pointer is aligned
    cr_assert((uintptr_t)x % align == 0, "x is not aligned");

    // Free the block
    dy_free(x);
    assert_free_block_count(0, 1);
}

Test(dyma_suite, memalign_9, .timeout = TEST_TIMEOUT) {
	/**
	 * Test allocating a 9 bit-aligned block.
     */
    size_t sz_x = 1024;
    size_t align = 9;
    void *x = dy_memalign(sz_x, align);
    cr_assert(x == NULL, "dy_memalign(1024, 9) != NULL");
    cr_assert(dy_errno == EINVAL, "dy_errno != EINVAL");
}

Test(dyma_suite, coalescing_flushed, .timeout = TEST_TIMEOUT) {
	/**
	 * Test coalescing from a flushed quick list to a preceding free block.
     */
    size_t sz_x = 1024;
    size_t sz_y = 64;
    void *x = dy_malloc(sz_x);
    assert_free_block_count(0, 1);
    
    // Fill the quick list for size 32
    void *ptrs[QUICK_LIST_MAX + 1];
	for (int i = 0; i <= QUICK_LIST_MAX; i++) {
		ptrs[i] = dy_malloc(sz_y);
		cr_assert(ptrs[i] != NULL, "dy_malloc(%d) == NULL", sz_y);
	}
	for (int i = 0; i < QUICK_LIST_MAX; i++) {
		dy_free(ptrs[i]);
	}
    assert_free_block_count(0, 1);
    assert_quick_list_block_count(64 + 8, QUICK_LIST_MAX);

    // Free the first block
    dy_free(x);
    assert_free_block_count(0, 2);
    assert_quick_list_block_count(64 + 8, QUICK_LIST_MAX);

    // Free the last block, causing quick list to be flushed
    dy_free(ptrs[QUICK_LIST_MAX]);
    assert_free_block_count(0, 2);
    assert_quick_list_block_count(64 + 8, 1);
}

Test(dyma_suite, malloc_some_to_small, .timeout = TEST_TIMEOUT) {
    /**
     * Test searching for a free block in a free list which contains some blocks which are too small.
     */
    size_t sz_x = 768;
    size_t sz_y = 32;
    size_t sz_z = 960;

    // Allocate a correctly sized block
    void *p1 = dy_malloc(sz_z);
    // Allocate a small block to prevent coalescing
    void *p2 = dy_malloc(sz_y);
    // Allocate a incorrectly sized block
    void *p3 = dy_malloc(sz_x);
    // Allocate another small block to prevent coalescing
    void *p4 = dy_malloc(sz_y);
    // Allocate a incorrectly sized block
    void *p5 = dy_malloc(sz_x);
    // Allocate another small block to prevent coalescing
    void *p6 = dy_malloc(sz_y);

    // Free the first, third, and fifth blocks
    dy_free(p1);
    dy_free(p3);
    dy_free(p5);
    assert_free_block_count(0, 4);
    assert_free_list_size(5, 3);

    // Try to allocate a block of size 960
    void *p7 = dy_malloc(sz_z);
    cr_assert(p7 != NULL, "dy_malloc(%d) == NULL", sz_z);
    assert_free_block_count(0, 3);
    assert_free_list_size(5, 2);

    // Try to allocate another block of size 960 (will pull from next free list)
    void *p8 = dy_malloc(sz_z);
    cr_assert(p8 != NULL, "dy_malloc(%d) == NULL", sz_z);
    assert_free_block_count(0, 3);
    assert_free_list_size(5, 2);
    assert_free_list_size(4, 1);

    // Free the remaining blocks
    dy_free(p2);
    dy_free(p4);
    dy_free(p6);
    dy_free(p7);
    dy_free(p8);
    assert_free_block_count(0, 4);
    assert_quick_list_block_count(sz_y + 8, 3);
}

Test(dyma_suite, memalign_enomem, .timeout = TEST_TIMEOUT) {
    /**
     * Test calling dy_memalign when all the memory has been allocated.
     */
    dy_errno = 0;
    while (dy_mem_grow() != NULL) {
        // Grow the heap
    }
    void *ptr = dy_memalign(sizeof(int), 16);

    cr_assert(ptr == NULL, "dy_malloc(1) != NULL");
    cr_assert(dy_errno == ENOMEM, "dy_errno != ENOMEM");
}

Test(dyma_suite, realloc_invalid_pointer, .timeout = TEST_TIMEOUT) {
    /**
     * Test calling dy_realloc with an invalid (null) pointer.
     */
    dy_errno = 0;
    void *ptr = dy_realloc(NULL, 1);

    cr_assert(ptr == NULL, "dy_realloc(NULL, 1) != NULL");
    cr_assert(dy_errno == EINVAL, "dy_errno != EINVAL");
}

Test(dyma_suite, free_invalid_pointer, .timeout = TEST_TIMEOUT, .signal = SIGABRT) {
    /**
     * Test calling dy_free with an invalid (null) pointer.
     */
    dy_free(NULL);
}
