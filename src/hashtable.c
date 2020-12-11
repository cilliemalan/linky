#include "hashtable.h"
#include "logging.h"
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

// buckets are allocated in sizes that are multiples of 64
#define BUCKET_SIZE_INC 64
#define MAXINT 2147483647
#define MININT (-2147483647)
// the offset value for each bucket is divided by the
// platform maximum alignment which is typically 16 bytes
#define OFFSET_INCREMENT sizeof(max_align_t)
#define CALC_OFFSET(root, ptr) ((ptrdiff_t)(((uint8_t *)(ptr)) - ((uint8_t *)(root))) / OFFSET_INCREMENT)
#define OFFSET_PTR(root, offset) (uint32_t *)((uint8_t *)(root) + ((ptrdiff_t)(offset)*OFFSET_INCREMENT))
#define OFFSET_PTR_SAFE(root, offset) ((root) && (offset) ? OFFSET_PTR(root, offset) : NULL)
#define INDEX_OF_FIRST_FREE_BIT(bitmap) (int32_t)(__builtin_ffs(~(int32_t)(bitmap)) - 1)

#define TABLE_INDEX(table, key) ((key) % (table)->options.num_buckets)
#define TABLE_OFFSET(table, key) (table)->root[TABLE_INDEX(table, key)]
#define TABLE_BUCKET(table, key) OFFSET_PTR_SAFE((table)->root, TABLE_OFFSET(table, key))

#define TABLE_VALUE_SIZE(table) round_up_value_size((table)->options.value_size + sizeof(uint32_t), sizeof(uint32_t))

struct hastable_s
{
    int32_t *root;
    hashtable_options_t options;
    bool must_free;
};

static size_t round_up_value_size(size_t x, size_t multiple)
{
    return (x + multiple - 1) & -multiple;
}

static void *default_hasthable_allocate_fn(size_t size)
{
    return calloc(size, 1);
}

static void *default_hasthable_reallocate_fn(void *ptr, size_t orig_size, size_t new_size)
{
    void *newmem = realloc(ptr, new_size);

    // clear the new memory
    if (newmem && orig_size < new_size)
    {
        memset((uint8_t *)newmem + orig_size, 0, new_size - orig_size);
    }

    return newmem;
}

static void default_hasthable_free_fn(void *ptr, size_t orig_size)
{
    free(ptr);
}

static uint32_t *increase_bucket_size(hashtable table, uint32_t key)
{
    uint32_t *result = NULL;
    if (table)
    {
        uint32_t *bucket = TABLE_BUCKET(table, key);
        if (bucket)
        {
            uint32_t bucketsize = bucket[0];

            // increase bucket size at least enough for one value and
            // a bitmap just in case a bitmap is needed
            uint32_t newsize = bucketsize + round_up_value_size(TABLE_VALUE_SIZE(table) + sizeof(uint32_t), BUCKET_SIZE_INC);
            assert(newsize < MAXINT);

            uint32_t *newbucket = table->options.reallocate(bucket, bucketsize, newsize);
            if (newbucket)
            {
                // update bucket size.
                // Size is tracked in uint32_t increments.
                newbucket[0] = newsize / sizeof(uint32_t);

                // update the offset
                if (newbucket != bucket)
                {
                    int32_t offset = CALC_OFFSET(table->root, newbucket);
                    assert(offset > MININT && offset < MAXINT);
                    // store the offset
                    TABLE_OFFSET(table, key) = (int32_t)offset;
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
            // the first 8 bytes of the value contains some bookkeeping
            size_t bucketsize = round_up_value_size(TABLE_VALUE_SIZE(table) + sizeof(uint32_t), BUCKET_SIZE_INC);
            assert(bucketsize < MAXINT);

            // allocate the initial value memory
            uint32_t *newbucket = (uint32_t *)table->options.allocate(bucketsize);

            if (newbucket)
            {
                // size of the bucket in bucket size increments
                // Size is tracked in uint32_t increments.
                newbucket[0] = (uint32_t)bucketsize / sizeof(uint32_t);

                // calculate and check the offset
                ptrdiff_t offset = CALC_OFFSET(table->root, bucket);
                assert(offset > MININT && offset < MAXINT);
                // store the offset
                TABLE_OFFSET(table, key) = (int32_t)offset;

                // return the new bucket
                result = bucket;
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
            else if (options->num_buckets < 64)
            {
                if (options->num_buckets)
                {
                    warn("The minimum number of buckets is 64. options->num_buckets will be set to 64");
                }
                options->num_buckets = 64;
            }

            // minimum value size
            if (options->value_size < 1)
            {
                options->value_size = 1;
            }

            if (bucket_memory)
            {
                table->root = bucket_memory;
                table->must_free = false;
            }
            else
            {
                table->root = (int32_t *)table->options.allocate(sizeof(int32_t) * options->num_buckets);
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

bool hashtable_set(hashtable table, uint32_t key, void **value)
{
    bool result = false;
    if (table && value)
    {
        uint32_t *bucket = TABLE_BUCKET(table, key);
        if (!bucket)
        {
            // allocate the bucket
            bucket = increase_bucket_size(table, key);
        }

        // we need to set the value in the first free spot we can find

        // track our offset into the bucket structure
        size_t bitmap_offset = 1;
        uint32_t *newvalue = NULL;
        while (bucket && !newvalue)
        {
            // the bucket allocation bitmap. Each bit means a span of value_size bytes.
            // 1 means the span is allocated. 0 means it's free.
            uint32_t bucketsize = bucket[0];
            uint32_t bitmap = bucket[bitmap_offset];

            int32_t freebit = INDEX_OF_FIRST_FREE_BIT(bitmap);
            if (freebit >= 0)
            {
                // there is a free value spot in index freebit
                uint32_t value_offset = bitmap_offset + 1 + ((TABLE_VALUE_SIZE(table) * freebit) / sizeof(uint32_t));

                // make sure the value does not extend past the end of the bucket
                uint32_t value_end_offset = value_offset + (TABLE_VALUE_SIZE(table) / sizeof(uint32_t));

                // increase bucket size if needed
                while (bucket && value_end_offset < bucketsize)
                {
                    bucket = increase_bucket_size(table, key);
                    if (bucket)
                    {
                        bucketsize = bucket[0];
                    }
                }

                if (bucket)
                {
                    // "allocate" the value
                    bucket[bitmap_offset] |= 1 << freebit;

                    // assign the value
                    newvalue = bucket + value_offset;
                    newvalue[0] = key;
                }
            }
            else
            {
                // the next 32 values are full
                assert(bitmap == 0xffffffff);
                // move to the next bitmap
                bitmap_offset += 1 + ((TABLE_VALUE_SIZE(table) * 32) / sizeof(uint32_t));

                // increase the bucket size if needed
                while (bucket && bitmap_offset < bucketsize)
                {
                    bucket = increase_bucket_size(table, key);
                    if (bucket)
                    {
                        bucketsize = bucket[0];
                    }
                }
            }
        }

        *value = newvalue ? newvalue + 1 : NULL;
        result = newvalue != NULL;
    }
    else
    {
        error("table or value was NULL");
    }

    return result;
}

static uint32_t *hashtable_find_value_container(hashtable table, uint32_t key, uint32_t **pbitmap, uint32_t *pindex)
{
    uint32_t *result = NULL;
    if (table)
    {
        uint32_t *bucket = TABLE_BUCKET(table, key);
        if (bucket)
        {
            // search for the value
            uint32_t bucketsize = bucket[0];
            uint32_t index = 1;
            while (!result && index < bucketsize)
            {
                uint32_t bitmap = bucket[index];
                for (uint32_t i = 0; i < 32 && !result; i++)
                {
                    if ((1 << i) & bitmap)
                    {
                        // there is something allocated in this slot
                        uint32_t valoffset = index + 1 + ((TABLE_VALUE_SIZE(table) * i) / sizeof(uint32_t));
                        if (valoffset < bucketsize)
                        {
                            uint32_t valkey = bucket[valoffset];
                            if (valkey == key)
                            {
                                // found!
                                result = bucket + valoffset;
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
                index += 1 + ((TABLE_VALUE_SIZE(table) * 32) / sizeof(uint32_t));
            }
        }
    }
    return result;
}

bool hashtable_get(hashtable table, uint32_t key, void **value)
{
    uint32_t *val = hashtable_find_value_container(table, key, NULL, NULL);
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
    uint32_t *val = hashtable_find_value_container(table, key, &pbitmap, &index);
    if (val)
    {
        // clear out the value container
        memset(val, 0, TABLE_VALUE_SIZE(table));
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
            uint32_t *bucket = TABLE_BUCKET(table, i);
            if (bucket)
            {
                // iterate through all the values in the bucket
                uint32_t bucketsize = bucket[0];
                uint32_t index = 1;
                while (index < bucketsize)
                {
                    uint32_t bitmap = bucket[index];
                    for (uint32_t i = 0; i < 32; i++)
                    {
                        if ((1 << i) & bitmap)
                        {
                            uint32_t valoffset = index + 1 + ((TABLE_VALUE_SIZE(table) * i) / sizeof(uint32_t));
                            uint32_t valend = valoffset + (TABLE_VALUE_SIZE(table) / sizeof(uint32_t));
                            if (valend <= bucketsize)
                            {
                                iterator(table, state, bucket[valoffset], &bucket[valoffset + 1]);
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
                uint32_t *bucket = TABLE_BUCKET(table, i);
                if (bucket)
                {
                    table->options.free(bucket, bucket[0]);
                }
            }

            if (table->must_free)
            {
                table->options.free(table->root, table->options.num_buckets * sizeof(int32_t));
            }
        }

        // free the whole table
        free(table);
    }
}