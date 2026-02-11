#include "my_malloc.h"

typedef struct block{
    size_t size;
    struct block * next; // the type of this pointer is struct block
} block_t;

#define META_SIZE sizeof(block_t)

