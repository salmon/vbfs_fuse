#include "mempool.h"

#define MAX_POOLS 128
#define PRE_ALLOC_NUM 16

//static pthread_rwlock_t pool_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t pool_global_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mempool mp[MAX_POOLS];
static unsigned int nr_pools = 0;
//static unsigned int last_pool = 0;

static int expand_mempool_size_unlock(struct mempool *pool, int expand_num)
{
	int i;
	struct mem_area *mem_area = NULL;
	int obj_size;

	obj_size = pool->obj_size;

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

	return 0;
}

static int shrink_mempool_size_unlocked(struct mempool *pool, int shrink_num)
{
	int i = 0, num = shrink_num;
	struct mem_area *mem_area = NULL, *tmp = NULL;

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

	return 0;
}

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

/*
static int destory_mempool(struct mempool *pool)
{
	return 0;
}
*/

static void *mempool_alloc(struct mempool *pool)
{
	void *p = NULL;
	struct mem_area *mem_area;
	int is_alloc = 0;

	pthread_mutex_lock(&pool->lock_mempool);

	if (pool->free_num == 0) {
		expand_mempool_size_unlock(pool, PRE_ALLOC_NUM);
	}

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
		if (pool->free_num == PRE_ALLOC_NUM && pool->total_num > PRE_ALLOC_NUM) {
			shrink_mempool_size_unlocked(pool, PRE_ALLOC_NUM);
		}
	}

	pthread_mutex_unlock(&pool->lock_mempool);

	return 0;
}

void mempool_status(void)
{

}

void *mp_valloc(unsigned int size)
{
	int i;
	char *p = NULL;

	pthread_mutex_lock(&pool_global_lock);
	for (i = 0; i < nr_pools; i ++) {
		if (mp[i].is_align != 1) {
			continue;
		}
		if (mp[i].obj_size == size) {
			break;
		}
	}
	nr_pools ++;
	add_mempool_unlocked(&mp[nr_pools], PRE_ALLOC_NUM, size, 1);
	pthread_mutex_unlock(&pool_global_lock);

	p = mempool_alloc(&mp[i]);

	return p;
}

void *mp_malloc(unsigned int size)
{
	int i;
	char *p = NULL;

	pthread_mutex_lock(&pool_global_lock);
	for (i = 0; i < nr_pools; i ++) {
		if (mp[i].is_align != 0) {
			continue;
		}
		if (mp[i].obj_size == size) {
			break;
		}
	}
	nr_pools ++;
	add_mempool_unlocked(&mp[nr_pools], PRE_ALLOC_NUM, size, 0);
	pthread_mutex_unlock(&pool_global_lock);

	p = mempool_alloc(&mp[i]);

	return p;
}

void mp_free(void *p, unsigned int size)
{
	int i, m = -1, n = -1;

	pthread_mutex_lock(&pool_global_lock);
	for (i = 0; i < nr_pools; i ++) {
		if (mp[i].is_align != 0) {
			if (mp[i].obj_size == size) {
				m = i;
			}
		} else {
			if (mp[i].obj_size == size) {
				n = i;
			}
		}
	}
	pthread_mutex_unlock(&pool_global_lock);

	if (m != -1) {
		mempool_free(&mp[m], p);
	}

	if (n != -1) {
		mempool_free(&mp[n], p);
	}
}
