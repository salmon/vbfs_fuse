#ifndef __MEMPOOL_H_
#define __MEMPOOL_H_

#include "utils.h"
#include "log.h"

struct mem_area {
	void *mem;
	//int nref;
	int used;

	struct list_head mem_list;
};

struct mempool {
	int total_num;
	int free_num;

	int obj_size;
	int is_align;

	pthread_mutex_t lock_mempool;
	struct list_head mem_area_list;
};

//int add_mempool(struct mempool *pool, int prealloc_num, int obj_size, int need_align);
//int destory_mempool(struct mempool *pool);

//int expand_mempool_size(struct mempool *pool, int expand_num);
//int shrink_mempool_size(struct mempool *pool, int shrink_num);

void *mp_malloc(unsigned int size);
void *mp_valloc(unsigned int size);
void mp_free(void *p);

void mempool_status(void);

#endif
