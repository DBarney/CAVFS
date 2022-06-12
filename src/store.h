/*
 * Copywrite (c) 2022 ThoughtPlot, LLC.
 */
#ifndef STORE_H
#define STORE_H
#include <hiredis/hiredis.h>

#define DBG(fmt,...) fprintf(stderr, "%s[%d]: %s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__),fflush(stderr)

#define STORE_ERROR 1
#define STORE_SHORT_READ 2

typedef struct store {
	const char* name;
	redisContext *redis;
	int lock;
	const char* lock_token;

	unsigned long long size;
	redisReply *blocks;
	unsigned long long generation;
	unsigned long long new_generation;

} store;

store* store_open(const char *name);
int store_delete(store *st);
int store_close(store* st);

int store_lock_generation(store *st);
int store_unlock_generation(store	*st);
int store_promote_generation(store	*st);

int store_lock(store *st);
int store_unlock(store*st);
int store_is_locked(store *st);

void store_truncate(store *st, unsigned long long size);
int store_length(store *st);

int store_write(store *st, const void *buf, size_t loc, size_t len);
int store_read(store *st, void *buf, size_t loc, size_t len);

#endif /* STORE_H */
