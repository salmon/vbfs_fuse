#include "mempool.h"

int mempool_init(struct mempool *pool, int prealloc_num, int obj_size, int need_align)
{
	return 0;
}

int mempool_alloc(struct mempool *mempool)
{
	return 0;
}

int mempool_free(struct mempool *mempool)
{
	return 0;
}

void mempool_status(struct mempool *mempool)
{

}

int expand_mempool_size(struct mempool *pool, int expand_num)
{
	return 0;
}

int shrink_mempool_size(struct mempool *pool, int shrink_num)
{
	return 0;
}
