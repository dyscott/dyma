#include "dyma.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * This file provides a simulated heap with a max size of around 4MB, 
 * without breaking any other calls to malloc and free (which would break the unit tests).
 */

static void *mem_start = NULL;
static void *mem_end = NULL;

/**
 * @return The starting address of the simulated heap.
 */
void *dy_mem_start() {
    return mem_start;
}

/**
 * @return The ending address of the simulated heap.
 */
void *dy_mem_end() {
    return mem_end;
}

/**
 * Utilized to increase the size of the simulated heap by one page.
 *
 * @return On success, returns a pointer to the start of the additional page.
 *         On error, NULL is returned.
 */
void *dy_mem_grow() {
    static int page_count = 0;
    if (mem_start == NULL) {
        // Allocate 1024 pages immediately
        mem_start = malloc(PAGE_SZ * 1024);
        if (mem_start == NULL) {
            return NULL;
        }
        mem_end = mem_start;
    }

    if (page_count >= 1024) {
        // Maximum number of pages reached
        return NULL;
    }
    page_count++;


    void *new_page = mem_end;
    mem_end += PAGE_SZ;
    return new_page;
}