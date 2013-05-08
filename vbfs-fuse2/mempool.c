#include "mempool.h"

#define MAX_POOLS 128

static pthread_rwlock_t pool_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static struct mempool mp[MAX_POOLS];
static unsigned int nr_pools = 0;
static unsigned int last_pool = 0;

static int add_mempool_unlocked(struct mempool *pool, int prealloc_num, int obj_size, int need_align)
{
	int i;
	struct mem_area *mem_area = NULL;

	memset(pool, 0, sizeof(struct mempool));
	INIT_LIST_HEAD(&pool->mem_area_list);

	for (i = 0; i < prealloc_num; i ++) {
		if ((mem_area = Malloc(sizeof(struct mem_area))) == NULL) {
			break;
		}

		if (need_align) {
			if ((mem_area->mem = Valloc(obj_size)) == NULL) {
				free(mem_area);
				mem_area = NULL;
				break;
			}
		} else {
			if ((mem_area->mem = Malloc(obj_size)) == NULL) {
				free(mem_area);
				mem_area = NULL;
				break;
			}
		}
		mem_area->used = 0;

		list_add(&mem_area->mem_list, &pool->mem_area_list);
	}

	pool->total_num = i;
	pool->free_num = i;
	pool->is_align = need_align;
	pool->obj_size = obj_size;
	pthread_mutex_init(&pool->lock_mempool, NULL);

	return 0;
}

static int destory_mempool(struct mempool *pool)
{
	return 0;
}

static void *mempool_alloc(struct mempool *pool)
{
	void *p = NULL;
	struct mem_area *mem_area;
	int is_alloc = 0;

	pthread_mutex_lock(&pool->lock_mempool);

	if (pool->free_num > 0) {
		list_for_each_entry(mem_area, &pool->mem_area_list, mem_list) {
			if (mem_area->used == 0) {
				mem_area->used = 1;
				p = mem_area->mem;
				is_alloc = 1;
			}
		}

		if (is_alloc) {
			pool->free_num --;
		} else {
			log_err("BUG\n");
		}
	}

	pthread_mutex_unlock(&pool->lock_mempool);

	return p;
}

static int mempool_free(struct mempool *pool, void *mem)
{
	int is_freed = 0;
	struct mem_area *mem_area;

	if (NULL == mem)
		return -1;

	pthread_mutex_lock(&pool->lock_mempool);

	list_for_each_entry(mem_area, &pool->mem_area_list, mem_list) {
		if (mem == mem_area->mem) {
			if (mem_area->used > 0) {
				is_freed = 1;
				mem_area->used = 0;
			}
			else {
				log_err("BUG\n");
			}

			break;
		}
	}

	if (is_freed) {
		pool->free_num ++;
	}

	pthread_mutex_unlock(&pool->lock_mempool);

	return 0;
}

void mempool_status(void)
{

}

int expand_mempool_size(struct mempool *pool, int expand_num)
{
	int i;
	struct mem_area *mem_area = NULL;
	int obj_size;

	obj_size = pool->obj_size;
	pthread_mutex_lock(&pool->lock_mempool);

	for (i = 0; i < expand_num; i ++) {
		if ((mem_area = Malloc(sizeof(struct mem_area))) == NULL) {
			break;
		}

		if (pool->is_align) {
			if ((mem_area->mem = Valloc(obj_size)) == NULL) {
				free(mem_area);
				mem_area = NULL;
				break;
			}
		} else {
			if ((mem_area->mem = Malloc(obj_size)) == NULL) {
				free(mem_area);
				mem_area = NULL;
				break;
			}
		}
		mem_area->used = 0;

		list_add_tail(&mem_area->mem_list, &pool->mem_area_list);
	}

	pool->total_num += i;
	pool->free_num += i;

	pthread_mutex_unlock(&pool->lock_mempool);

	return 0;
}

int shrink_mempool_size(struct mempool *pool, int shrink_num)
{
	int i = 0, num = shrink_num;
	struct mem_area *mem_area = NULL, *tmp = NULL;

	pthread_mutex_lock(&pool->lock_mempool);

	list_for_each_entry_safe(mem_area, tmp, &pool->mem_area_list, mem_list) {
		if (mem_area->used == 0) {
			if (mem_area->mem == NULL)
				log_err("BUG\n");
			free(mem_area->mem);
			mem_area->mem = NULL;
			list_del(&mem_area->mem_list);
			free(mem_area);

			i ++;
			if (-- num == 0) {
				break;
			}
		}
	}

	pool->total_num -= i;
	pool->free_num -= i;

	pthread_mutex_unlock(&pool->lock_mempool);

	return 0;
}

void *mp_valloc(unsigned int size)
{
	
	return NULL;
}

void *mp_malloc(unsigned int size)
{

	return NULL;
}

void mp_free(void *p)
{

}
