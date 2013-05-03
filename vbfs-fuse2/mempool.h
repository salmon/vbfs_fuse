#ifndef __MEMPOOL_H_
#define __MEMPOOL_H_

#include "utils.h"
#include "log.h"

struct mem_area {
	void *mem;
	int nref;

	struct list_head mem_list;
};

struct mempool {
	int total_num;
	int free_num;

	int obj_size;
	int need_align;

	pthread_mutex_t lock_mempool;
	struct list_head mem_area_list;
};

int mempool_init(struct mempool *pool, int prealloc_num, int obj_size, int need_align);
int expand_mempool_size(struct mempool *pool, int expand_num);
int shrink_mempool_size(struct mempool *pool, int shrink_num);

int mempool_alloc(struct mempool *mempool);
int mempool_free(struct mempool *mempool);

void mempool_status(struct mempool *mempool);

#endif
