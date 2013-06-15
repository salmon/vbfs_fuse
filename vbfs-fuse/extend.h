#ifndef __EXTEND_H__
#define __EXTEND_H__

#include "list.h"

#define LIST_CLEAN 0
#define LIST_DIRTY 1
#define LIST_SIZE 2

enum {
	B_READING = 0,
	B_WRITING = 1,
	B_DIRTY = 2,
};

#define EXTNO_HASH(eno,hash_bits)		\
	((((eno) >> hash_bits) ^ (eno)) &	\
	((1 << hash_bits) - 1))

#define MAX_AGE 10
#define CLEANUP_INTERVAL 5

struct queue {
	struct list_head lru[LIST_SIZE];
	unsigned long n_buffers[LIST_SIZE];

	struct hlist_head *cache_hash;
	int hash_bits;

	struct list_head reserved_buffers;
	unsigned need_reserved_buffers;

	pthread_t clean_tid;
	int clean_stop;
	pthread_mutex_t clean_lock;

	pthread_cond_t free_buffer_cond;
	pthread_mutex_t free_buffer_lock;

	pthread_mutex_t lock;
};

typedef void (*end_io_fn_t)(void *args);

struct extend_buf {
	struct hlist_node hash_list;
	struct list_head lru_list;

	uint32_t eno;
	char *data;

	int rw;
	int error;
	unsigned int list_mode;
	unsigned int hold_cnt;
	unsigned int state;
	unsigned long last_accessed;

	pthread_mutex_t lock;
	pthread_cond_t cond;

	end_io_fn_t end_io_fn;
	void *args;
	struct list_head data_list;
	struct queue *q;
};

void *extend_new(struct queue *q, uint32_t eno, struct extend_buf **bp);
void *extend_read(struct queue *q, uint32_t eno, struct extend_buf **bp);
void extend_mark_dirty(struct extend_buf *b);
void extend_put(struct extend_buf *b);
void extend_release(struct extend_buf *b);
int extend_write_dirty(struct extend_buf *b);

int queue_write_dirty(struct queue *q);
void queue_write_dirty_async(struct queue *q);
struct queue *queue_create(unsigned int reserved_buffers, int hash_bits);
void queue_destroy(struct queue *q);

#endif
