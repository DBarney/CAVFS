/*
 * Copywrite (c) 2022 ThoughtPlot, LLC.
 */
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>

#include "store.h"
#include "common.h"
#include "../lz4-1.9.3/lib/lz4.h"

store* store_open(const char *name) {
	store *s = malloc(sizeof(store));
	s->name = name;
	s->blocks = NULL;
	s->lock_token = NULL;
	s->generation = 0;
	s->new_generation = 0;
	s->size = 0;
	s->lock = 0;
	
	redisContext *c = redisConnect("192.168.0.211", 6379);
	if (!c) {
		return NULL;
	}
	if (c->err) {
		return NULL;
	}

	s->redis = c;
	return s;
}

int store_delete(store *st) {
	return 0;
}

int store_close(store *s) {
	if (s) {
		redisFree(s->redis);
		free(s);
	}
	return 0;
}

int store_lock(store *st) {
	if (!st->generation) {
		return 1;
	}
	// no recursive write locks are allowed
	if (st->lock == 1) {
		return 1;
	}

	const char* token = "value";
	redisReply *reply = redisCommand(st->redis, "set %s:gen:%lld:lock %s NX PX 10000",st->name, st->generation, token);
	if (!reply) {
		return 1;
	}

	if (reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
		// failed to aquire the lock
		return 1;
	}

	st->lock = 1;
	st->lock_token = token;
	freeReplyObject(reply);

	return 0;
}

int store_unlock(store*st) {
	if (!st->generation) {
		return 1;
	}
	if (st->lock == 0) {
		return 1;
	}

	DBG("going to call %s %lld %s %d",st->name, st->generation, st->lock_token, st->lock);
	redisReply *reply = redisCommand(st->redis, "fcall unlock 1 %s:gen:%lld:lock %s", st->name, st->generation, st->lock_token);

	if (!reply) {
		return 1;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		freeReplyObject(reply);
		return 1;
	}
	if (reply->integer == 0) {
		freeReplyObject(reply);
		return 1;
	}	
	freeReplyObject(reply);
	st->lock = 0;
	st->lock_token = NULL;
	return 0;
}

int store_is_locked(store *st) {
	return st->lock;
}

int store_lock_generation(store *st) {
	redisReply *gen = redisCommand(st->redis, "GeT %s:gen", st->name);
	if (!gen){
		return 1;
	}

	if (gen->type == REDIS_REPLY_NIL) {
		freeReplyObject(gen);
		st->blocks = malloc(sizeof(redisReply));
		st->blocks->type = REDIS_REPLY_ARRAY;
		st->blocks->elements = 0;
		st->blocks->element = NULL;
		st->generation = 1;
		return 0;
	} else if (gen->type != REDIS_REPLY_STRING) {
		freeReplyObject(gen);
		return 1;
	}
	// copy out the generation string
	unsigned long long generation = strtoul(gen->str, NULL, 10);
	freeReplyObject(gen);

	// check if we already have the generation cached
	// locally
	if (generation == st->generation) {
		return 0;
	}

	//redisReply *r = redisCommand(st->redis, "incr %s:gen:%lld:readers", st->name, generation);
	//if (!r) {
	//	return 1;
	//}
	//freeReplyObject(r);

	// clear out values because we are setting them
	if(st->blocks) {
		freeReplyObject(st->blocks);
		st->blocks = NULL;
		st->generation = 0;
	}
	
	st->blocks = redisCommand(st->redis, "lrange %s:gen:%lld 0 -1", st->name, generation);
	if (!st->blocks) {
		return 1;
	}
	st->generation = generation;
	redisReply *r = redisCommand(st->redis, "get %s:gen:%lld:size",st->name, st->generation);
	if (!r) {
		return 1;
	}
	if (r->type != REDIS_REPLY_STRING) {
		freeReplyObject(r);
		return 1;
	}
	st->size = strtoul(r->str, NULL, 10);
	DBG("parsed %lld from %*.s", st->size, r->len, r->str);
	freeReplyObject(r);
	return 0;
}

int store_unlock_generation(store *st) {
	if (st->lock == 1) {
		return 1;
	}
	if (st->generation == 0) {
		return 0;
	}
	//redisReply *r = redisCommand(st->redis, "decr %s:gen:%lld:readers", st->name, st->generation);
	//if (!r) {
	//	return 1;
	//}
	//freeReplyObject(r);
	return 0;
}

int store_promote_generation(store *st) {
	if (!st->lock) {
		return 1;
	}
	// no need to bump the generation if no changes were made
	if (!st->new_generation) {
		return 0;
	}

	// we need to upoad the new block list.
	char **cmds = malloc(sizeof(char**)*(st->blocks->elements + 2));
	size_t *lens = malloc(sizeof(size_t)*(st->blocks->elements + 2));

	cmds[0] = "rpush";
	lens[0] = 5;

	cmds[1] = malloc(sizeof(char)*256);
	snprintf(cmds[1],256,"%s:gen:%lld", st->name, st->new_generation);
	lens[1] = strlen(cmds[1]);

	for (int i = 0; i < st->blocks->elements; i ++) {
		cmds[i + 2] = st->blocks->element[i]->str;
		lens[i + 2] = st->blocks->element[i]->len;
	}	
	redisReply *r = redisCommandArgv(st->redis, st->blocks->elements + 2, (const char **)cmds, lens);
 	if (!r) {
		return 1;
	}
	freeReplyObject(r);

	r = redisCommand(st->redis, "set %s:gen:%lld:size %lld", st->name, st->new_generation, st->size);
	if (!r) {
		return 1;
	}
	freeReplyObject(r);

	r = redisCommand(st->redis, "set %s:gen %lld", st->name, st->new_generation);
	if (!r) {
		return 1;
	}
	st->new_generation = 0;
	freeReplyObject(r);
	return 0;
}

void store_truncate(store *st, unsigned long long size) {
	st->size = size;
}

int store_length(store *st){
	return st->size;
}

int store_write(store *st, const void *buf, size_t loc, size_t len) {
	if (!st->lock) {
		return 1;
	}
	if (!st->generation) {
		return 1;
	}
	if (!st->new_generation) {
		st->new_generation = st->generation + 1;
	}
	int block = loc / 4096;
	// need to check against the map to see if have space for this block
	if (st->blocks->elements <= block) {
		// create a new entry for the new block
		st->blocks->element = realloc(st->blocks->element,sizeof(redisReply*) * block + 1);
		st->blocks->elements = block + 1;
		redisReply *bl = malloc(sizeof(redisReply));

		bl->type = REDIS_REPLY_STRING;
		st->blocks->element[block] = bl;
	}
	// allocate space for a key
	// update the key for the block
	redisReply *bl = st->blocks->element[block];
	bl->len = 256;
	bl->str = malloc(sizeof(char*)*256);
	snprintf(bl->str, bl->len, "%s:gen:%lld:b:%x", st->name, st->new_generation, block);
	bl->len = strlen(bl->str);
	DBG("write %s",bl->str);

	char dst[4096];
	int size = LZ4_compress_default((const char*) buf, dst, len, 4096);
	if (!size) {
		DBG("compression failed");
		return 1;
	}
	redisReply *r = redisCommandArgv(st->redis, 3,
		(const char *[]){ "set", bl->str, dst },
		(const size_t[]){ 3, bl->len, size });
	if (!r) {
		return 1;
	}
	freeReplyObject(r);
	DBG("checking size %d %d %d",st->size, len,loc);
	if (st->size < len + loc) {
		st->size = len+loc;
	}
	return 0;
}

int store_read(store *st, void *buf, size_t loc, size_t len) {
	// there is also a file change counter at 24 with 16 len or so,
	// that should map directly to a generation!
	if (!st->generation) {
		if (loc == 0 && len == 100){
			DBG("header read");
			// initial check to read the header. may not have a generation.
			int err =	store_lock_generation(st);
			if (err) {
				return err;
			}
			err = store_read(st, buf, loc, len);
			if (err) {
				store_unlock_generation(st);
				return err;
			}
			return store_unlock_generation(st);
		}
		return 1;
	}
	int block = loc / 4096;
	int offset = loc % 4096;
	memset(buf, 0, len);
	
	// need to check against the map to see if we have this block
	if (st->blocks->elements <= block) {
		DBG("block does not exist");
		return STORE_SHORT_READ;
	}
	
	redisReply *r = redisCommandArgv(st->redis, 2,
		(const char *[]){ "get", st->blocks->element[block]->str},
		(const size_t[]){ 3, st->blocks->element[block]->len});
	if (!r) {
		return 1;
	}
	if (r->type != REDIS_REPLY_STRING) {
		return STORE_SHORT_READ;
	}
	char dst[4096];
	int size = LZ4_decompress_safe (r->str, dst, r->len, 4096);
	DBG("read %lld bytes from %s",size, st->blocks->element[block]->str);
	if (len < 100) {
		DBG("bytes: %*.s", len, dst);
	}
	if (len > size) {
		DBG("a short read");
		memcpy(buf, dst, size);
		return STORE_SHORT_READ;
	}
	DBG("copying from %d %d to the buffer",offset,len);
	memcpy(buf, dst + offset, len);
	return 0;
}

