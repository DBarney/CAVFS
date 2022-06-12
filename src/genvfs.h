/*
 * Copywrite (c) 2022 ThoughtPlot, LLC.
 */
#ifndef CAVFS_H
#define CAVFS_H
#include "store.h"

typedef struct gen {
	sqlite3_file base;

	store *st;
} gen;

#define DBG(fmt,...) fprintf(stderr, "%s[%d]: %s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__),fflush(stderr)

#endif /* CAVFS_H */
