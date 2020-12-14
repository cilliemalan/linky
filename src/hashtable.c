#include "hashtable.h"
#include "logging.h"
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

struct hastable_s
{
    // the main hashtable structure. Each entry in the
    // array is indexed by the key. The value of each
    // entry is an offset from root to the bucket object.
    int32_t *root;
    hashtable_options_t options;
    bool must_free;
};

// buckets are allocated in sizes that are multiples of 64
#define BUCKET_SIZE_INC 64
#define MAXINT 2147483647
#define MININT (-2147483647)

// the offset value for each bucket is divided by the
#define OFFSET_INCREMENT 16

static int32_t calc_offset(int32_t *root, uint32_t *bucket)
{
    ptrdiff_t byte_offset = (uint8_t *)(bucket) - (uint8_t *)(root);
    assert((byte_offset % OFFSET_INCREMENT) == 0);
    return byte_offset / OFFSET_INCREMENT;
}

static uint32_t *offset_ptr(int32_t *root, int32_t offset)
{
    assert(root && offset);
    return (uint32_t *)((uint8_t *)(root) + ((ptrdiff_t)(offset)*OFFSET_INCREMENT));
}

static uint32_t *offset_ptr_safe(int32_t *root, int32_t offset)
{
    if (root && offset)
    {
        return offset_ptr(root, offset);
    }
    else
    {
        return NULL;
    }
}

static int32_t index_of_first_free_bit(uint32_t bitmap)
{
    return (int32_t)(__builtin_ffs(~(int32_t)(bitmap)) - 1);
}

static int32_t table_index(hashtable table, int32_t key)
{
    assert(table);
    return key % table->options.num_buckets;
}

static uint32_t table_offset(hashtable table, int32_t key)
{
    assert(table);
    uint32_t index = table_index(table, key);
    return table->root[index];
}

static void set_table_offset(hashtable table, int32_t key, int32_t offset)
{
    assert(table && offset);
    uint32_t index = table_index(table, key);
    table->root[index] = offset;
}

static uint32_t *table_bucket(hashtable table, int32_t key)
{
    assert(table);
    int32_t offset = table_offset(table, key);
    return offset_ptr_safe((table)->root, offset);
}

static size_t round_up_to(size_t x, size_t multiple)
{
    // assert(number of bits in multiple == 1);
    assert(__builtin_popcount(multiple) == 1);
    return (x + multiple - 1) & -multiple;
}

static size_t table_item_size(hashtable table)
{
    size_t item_size = sizeof(uint32_t) + (table)->options.value_size;
    return round_up_to(item_size, sizeof(uint32_t));
}

static void *default_hasthable_allocate_fn(void *state, size_t size)
{
    void *ptr = calloc(size, 1);
    return ptr;
}

static void *default_hasthable_reallocate_fn(void *state, void *ptr, size_t orig_size, size_t new_size)
{
    void *newmem = realloc(ptr, new_size);

    // clear the new memory
    if (newmem && orig_size < new_size)
    {
        memset((uint8_t *)newmem + orig_size, 0, new_size - orig_size);
    }

    return newmem;
}

static void default_hasthable_free_fn(void *state, void *ptr, size_t orig_size)
{
    free(ptr);
}

static uint32_t *increase_bucket_size(hashtable table, uint32_t key)
{
    uint32_t *result = NULL;
    if (table)
    {
        uint32_t *bucket = table_bucket(table, key);
        if (bucket)
        {
            uint32_t bucketsize_words = bucket[0];
            uint32_t bucketsize_bytes = bucketsize_words * sizeof(uint32_t);

            // increase bucket size at least enough for one item and
            // a bitmap just in case a bitmap is needed
            uint32_t newsize_bytes = round_up_to(bucketsize_bytes + table_item_size(table) + sizeof(uint32_t), BUCKET_SIZE_INC);
            uint32_t newsize_words = newsize_bytes / sizeof(uint32_t);

            uint32_t *newbucket = (uint32_t *)table->options.reallocate(table->options.state, bucket, bucketsize_bytes, newsize_bytes);
            if (newbucket)
            {
                // update bucket size.
                // Size is tracked in uint32_t increments.
                newbucket[0] = newsize_words;

                // update the offset
                if (newbucket != bucket)
                {
                    int32_t offset = calc_offset(table->root, newbucket);
                    assert(offset > MININT && offset < MAXINT);
                    // store the offset
                    set_table_offset(table, key, offset);
                }

                // return the new bucket
                result = newbucket;
            }
            else
            {
                error("could not reallocate bucket");
            }
        }
        else
        {
            // the first 8 bytes of the bucket contains some bookkeeping
            uint32_t bucketsize_bytes = round_up_to(table_item_size(table) + sizeof(uint32_t), BUCKET_SIZE_INC);
            uint32_t bucketsize_words = bucketsize_bytes / sizeof(uint32_t);

            // allocate the initial bucket memory
            uint32_t *newbucket = (uint32_t *)table->options.allocate(table->options.state, bucketsize_bytes);

            if (newbucket)
            {
                // size of the bucket in bucket size increments
                // Size is tracked in uint32_t increments.
                newbucket[0] = bucketsize_words;

                // calculate and check the offset
                ptrdiff_t offset = calc_offset(table->root, newbucket);
                assert(offset > MININT && offset < MAXINT);
                // store the offset
                set_table_offset(table, key, offset);

                // return the new bucket
                result = newbucket;
            }
            else
            {
                error("could not allocate memory for bucket");
            }
        }
    }
    return result;
}

hashtable hashtable_create(hashtable_options_t *options, void *bucket_memory, uint32_t bucket_memory_size)
{
    hashtable result = NULL;

    // validate options
    bool options_valid = true;
    if (options)
    {
        bool has_allocate = !!options->allocate;
        bool has_reallocate = !!options->reallocate;
        bool has_free = !!options->free;

        // make sure they are either all set or all clear
        bool fns_valid = (has_allocate || has_reallocate || has_free) ==
                         (has_allocate && has_reallocate && has_free);
        if (!fns_valid)
        {
            error("hashtable functions must either all be set or all be NULL");
            options_valid = false;
        }

        if ((bucket_memory && !bucket_memory_size) || (!bucket_memory && bucket_memory_size))
        {
            error("both bucket_memory AND bucket_memory_size must be specified or NEITHER.");
            options_valid = false;
        }

        if (bucket_memory_size && (bucket_memory_size % sizeof(int32_t)) != 0)
        {
            errorf("bucket_memory_size must be a multiple of %d", sizeof(int32_t));
            options_valid = false;
        }

        if (bucket_memory_size && (bucket_memory_size < 64 * sizeof(int32_t)))
        {
            errorf("bucket_memory_size must be at least %d bytes", 64 * sizeof(int32_t));
            options_valid = false;
        }

        if (bucket_memory_size &&
            options->num_buckets != 0 &&
            bucket_memory_size != options->num_buckets * sizeof(int32_t))
        {
            warn("bucket_memory_size is not equal to options->num_buckets. options->num_buckets will be changed");
        }
    }

    if (options_valid)
    {
        // allocate hashtable object
        hashtable table = (hashtable)calloc(sizeof(struct hastable_s), 1);

        if (table)
        {
            if (options)
            {
                // copy in options
                memcpy(&table->options, options, sizeof(hashtable_options_t));
            }

            // assign defaults
            if (table->options.allocate == NULL)
            {
                table->options.allocate = default_hasthable_allocate_fn;
            }
            if (table->options.reallocate == NULL)
            {
                table->options.reallocate = default_hasthable_reallocate_fn;
            }
            if (table->options.free == NULL)
            {
                table->options.free = default_hasthable_free_fn;
            }

            if (bucket_memory_size)
            {
                options->num_buckets = bucket_memory_size / sizeof(int32_t);
            }
            // minimum number of buckets is 64
            else if (table->options.num_buckets < 64)
            {
                if (table->options.num_buckets)
                {
                    warn("The minimum number of buckets is 64. options->num_buckets will be set to 64");
                }
                table->options.num_buckets = 64;
            }

            // minimum value size
            if (table->options.value_size < sizeof(int32_t))
            {
                table->options.value_size = sizeof(int32_t);
            }

            if (bucket_memory)
            {
                table->root = (int32_t *)bucket_memory;
                table->must_free = false;
            }
            else
            {
                table->root = (int32_t *)table->options.allocate(table->options.state, sizeof(int32_t) * table->options.num_buckets);
                table->must_free = true;
            }

            if (table->root)
            {
                // table initialized
                result = table;
            }
            else
            {
                error("could not allocate memory for hashtable");
                free(table);
            }
        }
        else
        {
            error("could not allocate memory for hashtable");
        }
    }
    return result;
}

static uint32_t *hashtable_find_item_container(hashtable table, uint32_t key, uint32_t **pbitmap, uint32_t *pindex, bool create)
{
    uint32_t *result = NULL;
    if (table)
    {
        uint32_t *bucket = table_bucket(table, key);

        // create the bucket if we need to
        if (!bucket && create)
        {
            bucket = increase_bucket_size(table, key);
        }

        if (bucket)
        {
            // search for the right item
            uint32_t bucketsize_words = bucket[0];
            uint32_t index = 1;
            while (!result && index < bucketsize_words)
            {
                uint32_t bitmap = bucket[index];
                for (uint32_t i = 0; i < 32 && !result; i++)
                {
                    if ((1 << i) & bitmap)
                    {
                        // there is something allocated in this slot
                        uint32_t item_offset = index + 1 + ((table_item_size(table) * i) / sizeof(uint32_t));
                        if (item_offset < bucketsize_words)
                        {
                            uint32_t item_key = bucket[item_offset];
                            if (item_key == key)
                            {
                                // found!
                                result = bucket + item_offset;
                                if (pbitmap)
                                {
                                    *pbitmap = bucket + index;
                                }
                                if (pindex)
                                {
                                    *pindex = i;
                                }
                            }
                        }
                    }
                }
                index += 1 + ((table_item_size(table) * 32) / sizeof(uint32_t));
            }

            // if it wasn't found, create it if needed
            if (!result && create)
            {
                uint32_t index = 1;
                while (bucket && !result)
                {
                    // the bucket allocation bitmap. Each bit means a span of item_size bytes.
                    // 1 means the span is allocated. 0 means it's free.
                    uint32_t bucketsize_words = bucket[0];
                    uint32_t bitmap = bucket[index];

                    int32_t freebit = index_of_first_free_bit(bitmap);
                    if (freebit >= 0)
                    {
                        // there is a free item spot in index freebit
                        uint32_t item_offset = index + 1 + ((table_item_size(table) * freebit) / sizeof(uint32_t));

                        // make sure the item does not extend past the end of the bucket
                        uint32_t item_end_offset = item_offset + (table_item_size(table) / sizeof(uint32_t));

                        // increase bucket size if needed
                        while (bucket && item_end_offset > bucketsize_words)
                        {
                            bucket = increase_bucket_size(table, key);
                            if (bucket)
                            {
                                bucketsize_words = bucket[0];
                            }
                        }

                        if (bucket)
                        {
                            // "allocate" the item
                            bucket[index] |= 1 << freebit;

                            // assign the item
                            result = bucket + item_offset;
                            result[0] = key;
                        }
                    }
                    else
                    {
                        // the next 32 items are full
                        assert(bitmap == 0xffffffff);
                        // move to the next bitmap
                        index += 1 + ((table_item_size(table) * 32) / sizeof(uint32_t));

                        // increase the bucket size if needed
                        while (bucket && index > bucketsize_words)
                        {
                            bucket = increase_bucket_size(table, key);
                            if (bucket)
                            {
                                bucketsize_words = bucket[0];
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

bool hashtable_get(hashtable table, uint32_t key, void **value, bool create)
{
    uint32_t *val = hashtable_find_item_container(table, key, NULL, NULL, create);
    if (val && value)
    {
        *value = val + 1;
    }

    return !!val;
}

bool hashtable_delete(hashtable table, uint32_t key)
{
    uint32_t *pbitmap;
    uint32_t index;
    uint32_t *val = hashtable_find_item_container(table, key, &pbitmap, &index, false);
    if (val)
    {
        // clear out the value container
        memset(val, 0, table_item_size(table));
        // clear the bit
        *pbitmap &= ~(1 << index);
        // TODO: shrink the bucket if needed
    }
    return !!val;
}

void hashtable_iterate(hashtable table, hashtable_iterate_fn iterator, void *state)
{
    if (table && iterator)
    {
        for (uint32_t i = 0; i < table->options.num_buckets; i++)
        {
            // iterate through all the buckets
            uint32_t *bucket = table_bucket(table, i);
            if (bucket)
            {
                // iterate through all the values in the bucket
                uint32_t bucketsize_words = bucket[0];
                uint32_t index = 1;
                while (index < bucketsize_words)
                {
                    uint32_t bitmap = bucket[index];
                    for (uint32_t i = 0; i < 32; i++)
                    {
                        if ((1 << i) & bitmap)
                        {
                            uint32_t item_offset = index + 1 + ((table_item_size(table) * i) / sizeof(uint32_t));
                            uint32_t item_end = item_offset + (table_item_size(table) / sizeof(uint32_t));
                            if (item_end <= bucketsize_words)
                            {
                                iterator(table, state, bucket[item_offset], &bucket[item_offset + 1]);
                            }
                        }
                    }
                }
            }
        }
    }
}

void hashtable_free(hashtable table)
{
    if (table)
    {
        if (table->root)
        {
            // free all allocated buckets
            for (uint32_t i = 0; i < table->options.num_buckets; i++)
            {
                uint32_t *bucket = table_bucket(table, i);
                if (bucket)
                {
                    uint32_t bucketsize_words = bucket[0];
                    uint32_t bucketsize_bytes = bucketsize_words * sizeof(uint32_t);
                    table->options.free(table->options.state, bucket, bucketsize_bytes);
                }
            }

            if (table->must_free)
            {
                table->options.free(table->options.state, table->root, table->options.num_buckets * sizeof(int32_t));
            }
        }

        // free the whole table
        free(table);
    }
}