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

#endif /* CAVFS_H */
