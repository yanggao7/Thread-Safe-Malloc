#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// Thread Safe malloc/free: locking version
void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);

// Thread Safe malloc/free: non-locking version
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

#endif
