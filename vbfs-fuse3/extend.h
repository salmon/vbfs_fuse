#ifndef __EXTEND_H__
#define __EXTEND_H__

#include "utils.h"
#include "super.h"
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
	int inode_ref; /* used by inode_vbfs */

	pthread_mutex_t ed_lock;

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

int write_to_disk(int fd, void *buf, __u64 offset, size_t len);

int read_from_disk(int fd, void *buf, __u64 offset, size_t len);

struct extend_data *open_edata(const __u32 extend_no, \
			struct extend_queue *equeue, int *ret);

int close_edata(struct extend_data *edata);

int sync_edata(struct extend_data *edata);

int read_edata(struct extend_data *edata);

#endif
