#include <stdlib.h>

#ifndef SWAP_H
#define SWAP_H

void swap_init(void);

size_t swap_frame(void*);

void restore_frame(void*, size_t);

/* void free_frame(size_t);*/

#endif /*vm/swap.h*/
