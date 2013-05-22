#include "log.h"
#include "io-thread.h"

struct extend_queue extend_bm_eq;
struct extend_queue inode_bm_eq;
struct extend_queue inode_eq;
struct extend_queue data_eq;

static struct thread_info info;

static void *extend_worker_fn(void *args)
{
	struct extend_data *edata = NULL;

	while (! info.stop) {
		pthread_mutex_lock(&info.pending_lock);

		while (list_empty(&info.pending_list)) {
			pthread_cond_wait(&info.pending_cond, &info.pending_lock);
			if (info.stop) {
				pthread_mutex_unlock(&info.pending_lock);
				pthread_exit(NULL);
			}
		}
		edata = list_first_entry(&info.pending_list, struct extend_data, data_list);

		clear_extend_queued(edata);
		list_del(&edata->data_list);

		pthread_mutex_unlock(&info.pending_lock);

		info.request_fn(edata);
	}

	pthread_exit(NULL);
}

int rw_thread_open(request_func_t rfn, int nr_threads)
{
	int i = 0, ret = 0;

	info.worker_thread = malloc(sizeof(pthread_t) * nr_threads);
	memset(info.worker_thread, 0, sizeof(pthread_t) * nr_threads);

	info.request_fn = rfn;
	INIT_LIST_HEAD(&info.pending_list);

	pthread_cond_init(&info.pending_cond, NULL);
	pthread_mutex_init(&info.pending_lock, NULL);
	pthread_mutex_init(&info.startup_lock, NULL);

	pthread_mutex_lock(&info.startup_lock);
	for (i = 0; i < nr_threads; i++) {
		ret = pthread_create(&info.worker_thread[i], NULL,
				extend_worker_fn, NULL);
		if (ret) {
			printf("pthread_create failed\n");
			goto destroy_threads;
		}
	}
	pthread_mutex_unlock(&info.startup_lock);

	info.nr_threads = nr_threads;

	return 0;

destroy_threads:
	info.stop = 1;

	pthread_mutex_unlock(&info.startup_lock);
	for (; i > 0; i--) {
		pthread_join(info.worker_thread[i - 1], NULL);
	}

	pthread_cond_destroy(&info.pending_cond);
	pthread_mutex_destroy(&info.pending_lock);
	pthread_mutex_destroy(&info.startup_lock);
	free(info.worker_thread);

	return -1;
}

void rw_thread_close()
{
	int i = 0;

	log_err("close io threads\n");
	sleep(10);
	info.stop = 1;
	pthread_cond_broadcast(&info.pending_cond);

	for (i = 0; info.worker_thread[i] && i < info.nr_threads; i ++)
		pthread_join(info.worker_thread[i], NULL);

	pthread_cond_destroy(&info.pending_cond);
	pthread_mutex_destroy(&info.pending_lock);
	pthread_mutex_destroy(&info.startup_lock);
	free(info.worker_thread);

	info.stop = 0;
}

int extend_submit(struct extend_data *edata)
{
	pthread_mutex_lock(&info.pending_lock);

	if (! extend_queued(edata)) {
		set_extend_queued(edata);
		list_add_tail(&edata->data_list, &info.pending_list);
	} else {
		pthread_mutex_unlock(&info.pending_lock);
		return 0;
	}

	pthread_cond_signal(&info.pending_cond);

	pthread_mutex_unlock(&info.pending_lock);

	return 0;
}

static void init_equeue(struct extend_queue *equeue)
{
	INIT_LIST_HEAD(&equeue->all_ed_list);
	pthread_mutex_init(&equeue->all_ed_lock, NULL);
}

static int extend_queue_init()
{
	init_equeue(&extend_bm_eq);
	init_equeue(&inode_bm_eq);
	init_equeue(&inode_eq);
	init_equeue(&data_eq);

	return 0;
}

int rw_thread_init()
{
	memset(&info, 0, sizeof(struct thread_info));
	extend_queue_init();

	return 0;
}
