#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// This hashtable implementation uses integer keys and fixed size values.
// The hashtable root structure is an array of pointers to buckets. The keys
// are straightforward indices into this structure. Once an item is added
// the array is indexed and a bucket structure created if necessary.
// in order to be compatible with our memory mapped structures, instead
// of pointers, 32-bit offsets are used so care must be taken to ensure that
// pointers for buckets and the bucket structure itself don't differ by
// more than 2Gb of memory space.
// when a bucket is full, it will be reallocated rather than doing some
// linked list shenanigans.


typedef struct hastable_s* hashtable;

typedef void* (*hasthable_allocate_fn)(size_t size);
typedef void* (*hasthable_reallocate_fn)(void* ptr, size_t orig_size, size_t new_size);
typedef void (*hasthable_free_fn)(void* ptr, size_t orig_size);
typedef bool (*hashtable_iterate_fn)(hashtable table, void* state, uint32_t key, void* value);

struct hashtable_options_s {
    // a function the hashtable will call to allocate memory
    // note the hashtable itself is allocated with malloc
    hasthable_allocate_fn allocate;

    // a function the hashtable will call to reallocate memory
    hasthable_reallocate_fn reallocate;

    // a function the hashtable will call to free memory
    hasthable_free_fn free;

    // the number of 32-bit sized bukets to allocate.
    size_t num_buckets;

    // the amount of space to allocate for each value
    size_t value_size;
};
typedef struct hashtable_options_s hashtable_options_t;

// create a new hashtable
hashtable hashtable_create(hashtable_options_t* options, void* bucket_memory, uint32_t bucket_memory_size);

// adds an item to the hashtable and allocates space for the
// value. The pointer to the memory for the value is put in value. The size
// of the space allocated for the value is specified in options
// when creating the hashtable.
bool hashtable_set(hashtable table, uint32_t key, void** value);

// gets a value from the hashtable for the specified the key.
// a pointer to the value is put in value. It will be set
// to NULL if the key is not found.
bool hashtable_get(hashtable table, uint32_t key, void** value);

// removes a key and value from the hashtable for the specified the key.
bool hashtable_delete(hashtable table, uint32_t key);

// iterates over all the keys and values of a hashtable. If the
// iterator function returns false the iteration will stop.
void hashtable_iterate(hashtable table, hashtable_iterate_fn iterator, void* state);

// free a hashtable
void hashtable_free(hashtable table);

