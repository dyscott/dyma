#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "dyma.h"

#define MIN_BLOCK_SIZE 32
#define ROW_SIZE 8

#define GET_ALLOC(bp) (((bp)->header) & THIS_BLOCK_ALLOCATED)
#define GET_PREV_ALLOC(bp) (((bp)->header) & PREV_BLOCK_ALLOCATED)
#define GET_IN_QUICK_LIST(bp) (((bp)->header) & IN_QUICK_LIST)
#define GET_SIZE(bp) (((bp)->header) & ~0x7)
#define GET_FOOTER_PTR(bp) (((void *)bp + GET_SIZE(bp) - ROW_SIZE))

#define SET_ALLOC(bp) ((bp)->header |= THIS_BLOCK_ALLOCATED)
#define SET_PREV_ALLOC(bp) ((bp)->header |= PREV_BLOCK_ALLOCATED)
#define SET_IN_QUICK_LIST(bp) ((bp)->header |= IN_QUICK_LIST)
#define SET_SIZE(bp, size) ((bp)->header = size | (bp->header & 0x7))

#define CLEAR_HEADER(bp) ((bp)->header = 0)
#define CLEAR_ALLOC(bp) ((bp)->header &= ~THIS_BLOCK_ALLOCATED)
#define CLEAR_PREV_ALLOC(bp) ((bp)->header &= ~PREV_BLOCK_ALLOCATED)
#define CLEAR_IN_QUICK_LIST(bp) ((bp)->header &= ~IN_QUICK_LIST)
#define CLEAR_SIZE(bp) ((bp)->header &= 0x7)

int calc_min_free_list_index(size_t size);
int calc_quick_list_index(size_t size);
size_t calc_block_size(size_t size);

dy_block* create_block(void *start, size_t size);
void insert_block_free_list(dy_block *block);
dy_block *split_block(dy_block *block, size_t size);
void alloc_block(dy_block *block);
void dealloc_block(dy_block *block);
dy_block *coalesce_prev_block(dy_block *block);
void flush_quick_list(int index);

int init_heap();
dy_block *get_quick_list_block(size_t block_size);
dy_block *get_free_list_block(size_t block_size);
dy_block *get_heap_block(size_t block_size);
int check_pointer(void *pp);
int free_to_quick_list(dy_block *block);
void free_to_free_list(dy_block *block);