# Dyma - Dynamic Memory Allocator

A dynamic memory allocator written in C. This was originally a project for my System Fundamentals II class but I have made some improvements to it since then.

## Design

Dyma is a segregated free list allocator, using separate free lists for different size classes of blocks. Within these free lists, Dyma uses a first-fit placement policy. During allocation, Dyma will split blocks if the remainder is large enough to be a free block. Free blocks have footers storing their size, enabling Dyma to coalesce adjacent free blocks.

Dyma also makes use of "quick lists" as an optimization, delaying the coalescing of free blocks that are likely to be allocated again soon. Specifically, blocks of a small size are sent to a quick list for its exact size, allowing for O(1) allocation and freeing of these blocks. However, once the quick list reaches capacity, the blocks are returned to the main free list and coalesced if possible.

*Note: To avoid conflicts with existing libraries, such as [criterion](https://github.com/Snaipe/Criterion) which I used for unit tests, Dyma simulates a heap of a size of ~4MB by making a large allocation using `malloc` at first use. As such, Dyma is not suitable for use in an actual program and is only meant for learning purposes.*

## Usage

Dyma provides the following functions for use:

```c
void *dy_malloc(size_t size);
void *dy_free(void *ptr);
void *dy_realloc(void *ptr, size_t size);
void *dy_memalign(size_t size, size_t align);
```

`dy_malloc` and `dy_free` provide the interface for allocating and freeing memory. `dy_realloc` is used to resize an existing allocation. `dy_memalign` is used to allocate memory with a specified alignment (must be a power of 2) for scenarios where the default alignment of 8 bytes is not sufficient.

## Building

Dyma can be built using the provided Makefile using `make clean all` or `make clean debug` for a debug build.

## Testing

Dyma comes with a test suite that can be run using `bin/dyma_tests`. The test suite uses [criterion](https://github.com/Snaipe/Criterion).

## Acknowledgements

While writing Dyma, [*Computer Systems: A Programmer's Perspective*](http://csapp.cs.cmu.edu/3e/home.html) by Randal E. Bryant and David R. O'Hallaron was an invaluable resource.