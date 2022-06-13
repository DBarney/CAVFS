/*
 * Copywrite (c) 2022 ThoughtPlot, LLC.
 */
#ifndef COMMON_H
#define COMMON_H
#ifdef DEBUG
#define DBG(fmt,...) fprintf(stderr, "%s[%d]: %s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__),fflush(stderr)
#else
#define DBG(fmt,...)  do{}while(0)
#endif

#endif /* COMMON_H */
