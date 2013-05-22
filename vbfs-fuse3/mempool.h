#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include "utils.h"
#include "log.h"

void *Valloc(unsigned int size);
void *Malloc(unsigned int size);

void *mp_malloc(unsigned int size);
void *mp_valloc(unsigned int size);
void mp_free(void *p);

#endif
