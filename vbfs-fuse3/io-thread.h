#ifndef __IO_THREAD_H__
#define __IO_THREAD_H__

#include "extend.h"
#include "direct-io.h"

typedef int (*request_func_t)(struct extend_data *edata);

struct thread_info {
	pthread_t *worker_thread;
	int nr_threads;

	pthread_cond_t pending_cond;
	pthread_mutex_t pending_lock;
	struct list_head pending_list;

	pthread_mutex_t startup_lock;

	int stop;

	request_func_t request_fn;
};


int rw_thread_init();

int extend_submit(struct extend_data *edata);

int rw_thread_open(request_func_t rfn, int nr_threads);

void rw_thread_close();

#endif
