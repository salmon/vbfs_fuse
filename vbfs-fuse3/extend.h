#ifndef __EXTEND_H__
#define __EXTEND_H__

#include "utils.h"
#include "list.h"

enum {
	BUFFER_NOT_READY,
	BUFFER_CLEAN,
	BUFFER_DIRTY,
};

/*
 * extend_bm_queue
 * inode_bm_queue
 * inode_queue
 * data_queue
 * */
struct extend_queue {
	/* all extend_data in list */
	struct list_head all_ed_list;
	pthread_mutex_t all_ed_lock;

#if 0
/* useless */
	/* wait to read/write */
	struct list_head pending_list;
	pthread_mutex_t pending_lock;
	pthread_cond_t pending_cond;

	/* using */
	struct list_head finish_list;
	pthread_mutex_t finish_lock;
/* end */
#endif

	/* nobody reference it, but not free immediately */
	struct list_head cache_list;
	pthread_mutex_t cache_lock;

	void *q_private;
};

struct extend_data {
	__u32 extend_no;
	char *buf;

	int status; /* buf status: 0->not ready 1->clean 2->dirty */
	int ref;
	unsigned long flags;
	int err_no;

	pthread_mutex_t ed_lock;
	pthread_cond_t ed_cond;

	struct list_head data_list;
	struct list_head rq_list;

	struct extend_queue *equeue;
};

struct queue_ops {
	int (*open)(struct extend_queue *rq, __u32 extend_no);
	int (*close)(struct extend_queue *rq);
	int (*write)(struct extend_queue *rq);
	int (*read)(struct extend_queue *rq);
	int (*mark_dirty)(struct extend_queue *rq);
};

enum {
	EXTEND_FLAGS_QUEUED,
	EXTEND_FLAGS_PROCESSED,
	EXTEND_FLAGS_ASYNC,
};

#define EXTEND_FLAGS(bit, name)						\
static inline void set_extend_##name(struct extend_data *ed)		\
{									\
	(ed)->flags |= (1UL << EXTEND_FLAGS_##bit);			\
}									\
static inline void clear_extend_##name(struct extend_data *ed)		\
{									\
	(ed)->flags &= ~(1UL << EXTEND_FLAGS_##bit);			\
}									\
static inline int extend_##name(const struct extend_data *ed)		\
{									\
	return ((ed)->flags & (1UL << EXTEND_FLAGS_##bit));		\
}

EXTEND_FLAGS(QUEUED, queued)
EXTEND_FLAGS(PROCESSED, processed)
EXTEND_FLAGS(ASYNC, async)

struct extend_data *open_edata(const __u32 extend_no, \
			struct extend_queue *equeue, int *ret);

int close_edata(struct extend_data *edata);

int write_edata(struct extend_data *edata);

int read_edata(struct extend_data *edata);

#endif
