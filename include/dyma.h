#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

int dy_errno;

#define THIS_BLOCK_ALLOCATED  0x1
#define PREV_BLOCK_ALLOCATED  0x2
#define IN_QUICK_LIST         0x4

typedef size_t dy_header;
typedef size_t dy_footer;

typedef struct dy_block {
    dy_header header;
    union {
        struct {
            struct dy_block *next;
            struct dy_block *prev;
        } links;
        char payload[0];
    } body;
} dy_block;

#define NUM_QUICK_LISTS 20
#define QUICK_LIST_MAX   5
struct {
    int length;
    struct dy_block *first;
} dy_quick_lists[NUM_QUICK_LISTS];

#define NUM_FREE_LISTS 10
struct dy_block dy_free_list_heads[NUM_FREE_LISTS];

void *dy_malloc(size_t size);
void *dy_realloc(void *ptr, size_t size);
void dy_free(void *ptr);
void *dy_memalign(size_t size, size_t align);

void *dy_mem_start();
void *dy_mem_end();
void *dy_mem_grow();
#define PAGE_SZ ((size_t)4096)
