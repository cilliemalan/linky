#include "database.h"
#include "logging.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <memory.h>

#include <zlib.h>

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define HUGE_PAGE_SIZE 2097152
#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

union mm_extent
{
    uint8_t data[64];
    uint32_t words[16];
};

struct mm_index_record
{
    uint32_t bitmap[1024];
    bool full;
    uint32_t extents_allocated;
    uint32_t checksum;
};

union mm_page
{
    uint8_t data[1024 * 2048];
    union mm_extent extents[32768];
};

struct mm_index_page
{
    struct mm_index_record indices[sizeof(union mm_page) / sizeof(struct mm_index_record)];
};

_Static_assert(sizeof(union mm_extent) == 64, "extent must be 64 bytes");
_Static_assert(sizeof(union mm_page) == HUGE_PAGE_SIZE, "page must be 2Mb");

struct database_s
{
    int fd;
    const char *file;
    union mm_page *data;
    uint64_t size;
};

// open or create the database file
bool mm_open(database db)
{
    return false;
}

// increase the size of the database file by 2MB
bool mm_sbrk(database db)
{
    return false;
}

void *mm_allocate(database db, size_t size)
{
    return NULL;
}

void *mm_reallocate(database db, void *ptr, size_t orig_size, size_t new_size)
{
    return NULL;
}

void mm_free(database db, void *ptr, size_t orig_size)
{
}

database database_open(const char *file, bool create, gid_t gid, uid_t uid)
{
    int fperm = S_IRUSR | S_IWUSR;

    // open the file
    int fd = open(file, O_CREAT, fperm);
    if (fd == -1)
    {
        errorf("Could not open file %s", file);
        errorp();
        close(fd);
        return NULL;
    }

    // lock the file
    if (flock(fd, LOCK_EX) == -1)
    {
        errorf("Could not lock file %s", file);
        errorp();
        close(fd);
        return NULL;
    }

    // check file size, owner and mode
    struct stat fst;
    if (fstat(fd, &fst) == -1)
    {
        errorf("Could not open file %s", file);
        errorp();
        close(fd);
        return NULL;
    }

    // make sure the file size is a multiple of page size
    if ((fst.st_size % HUGE_PAGE_SIZE) != 0)
    {
        errorf("The database file %s has an incorrect size (%lld). The size must be a multiple of %d bytes",
               file,
               (int64_t)fst.st_size,
               HUGE_PAGE_SIZE);
        close(fd);
        return NULL;
    }

    // fix file owner if needed
    if (fst.st_gid != gid || fst.st_uid != uid)
    {
        warnf("changing file %s ownership from %d:%d to %d:%d", file, fst.st_uid, fst.st_gid, uid, gid);
        if (fchown(fd, uid, gid) == -1)
        {
            errorf("Could not change file %s owner", file);
            errorp();
            close(fd);
            return NULL;
        }
    }

    // fix mode if needed. Only owner has permission to read or write
    if (fst.st_mode & (S_IRWXG | S_IRWXO))
    {
        warnf("changing file %s mode from %04o to %04o", file, fst.st_mode, fperm);
        if (fchmod(fd, fperm) == -1)
        {
            errorf("Could not change file %s mode", file);
            errorp();
            close(fd);
            return NULL;
        }
    }

    // allocate at least four pages.
    size_t filesize = fst.st_size;
    if (filesize < HUGE_PAGE_SIZE * 4)
    {
        if (ftruncate(fd, HUGE_PAGE_SIZE * 4) == -1)
        {
            errorf("Could not increase file %s size to %d Mb", file, (HUGE_PAGE_SIZE * 4) / 1024 / 1024);
            errorp();
            close(fd);
            return NULL;
        }
        filesize = HUGE_PAGE_SIZE * 4;
    }

    // map the file
    void *filemem = mmap(
        NULL, // address hint
        filesize,
        PROT_READ | PROT_WRITE,
        MAP_HUGE_2MB | MAP_SHARED,
        fd,
        0); // offset

    if (!filemem)
    {
        errorf("Could not map file %s", file);
        errorp();
        close(fd);
        return NULL;
    }

    database db = (database)malloc(sizeof(struct database_s));
    memset(db, 0, sizeof(struct database_s));
    db->data = filemem;
    db->fd = fd;
    size_t fnlen = strlen(file) + 1;
    char* filenamemem = (char*) malloc(fnlen);
    memcpy(filenamemem, file, fnlen);
    db->file = filenamemem;
    db->size = filesize;

    return db;
}

bool database_get(database db, uint32_t key, const char **value, uint64_t *expires)
{
    return false;
}

bool database_set(database db, uint32_t key, const char *value, uint64_t expires)
{
    return false;
}

void database_close(database db)
{
    if (db)
    {
        if (db->fd > 0)
        {
            close(db->fd);
            db->fd = -1;
        }
        if (db->file)
        {
            free((char*)db->file);
            db->file = NULL;
        }
        db->size = 0;
        db->data = NULL;
        free(db);
    }
}