#pragma once
#include <stdlib.h>

typedef void* (*allocator_sbrk)();

typedef struct allocator_s *allocator;

// create a new allocator given an sbrk function
allocator allocator_create(allocator_sbrk);

// allocate some memory using an allocator
void* allocator_malloc(allocator, size_t size);

// increase or decrease the size of some memory using an allocator
void* allocator_realloc(allocator, void* ptr, size_t original_size, size_t size);

// free some memory using an allocator
void* allocator_free(allocator, void* ptr, size_t original_size);
