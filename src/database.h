#pragma once
#include <stdint.h>
#include <stdbool.h>

#include <sys/types.h>

typedef struct database_s* database;

// open or create the database 
database database_open(const char *file, bool create, gid_t gid, uid_t uid);

// get a value from the database
bool database_get(database db, uint32_t key, const char** value, uint64_t* expires);

// set a value in the database
bool database_set(database db, uint32_t key, const char* value, uint64_t expires);

// close the database
void database_close(database db);