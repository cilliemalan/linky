#include "allocator.h"


struct allocator_s {
    allocator_sbrk sbrk;
    void* memory;
};
