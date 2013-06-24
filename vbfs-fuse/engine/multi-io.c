#include "../utils.h"
#include "../log.h"
#include "../ioengine.h"

#define NR_THREAD 5

struct thread_info {
	pthread_t *worker_thread;
	int nr_threads;

	pthread_cond_t pending_cond;
	pthread_mutex_t pending_lock;
	struct list_head pending_list;

	pthread_mutex_t startup_lock;

	int stop;
};

static struct thread_info info;

static void extend_bufio(struct extend_buf *b)
{
	int ret = 0;

	if (WRITE == b->rw) {
		ret = write_extend(b->real_eno, b->data);
	} else if (READ == b->rw) {
		ret = read_extend(b->real_eno, b->data);
	} else {
		b->error = -EINVAL;
	}
	if (ret < 0)
		b->error = -errno;
}

static void *extend_worker_fn(void *args)
{
	struct extend_buf *b = NULL;

	while (! info.stop) {
		pthread_mutex_lock(&info.pending_lock);

		while (list_empty(&info.pending_list)) {
			pthread_cond_wait(&info.pending_cond, &info.pending_lock);
			if (info.stop) {
				pthread_mutex_unlock(&info.pending_lock);
				pthread_exit(NULL);
			}
		}
		b = list_first_entry(&info.pending_list, struct extend_buf, data_list);

		list_del(&b->data_list);

		pthread_mutex_unlock(&info.pending_lock);

		extend_bufio(b);
		b->end_io_fn(b);
	}

	pthread_exit(NULL);
}

int rdwr_thread_open(int nr_threads)
{
	int i = 0, ret = 0;

	info.worker_thread = malloc(sizeof(pthread_t) * nr_threads);
	memset(info.worker_thread, 0, sizeof(pthread_t) * nr_threads);

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

static int rdwr_exit()
{
	int i = 0;

	log_err("close io threads\n");
	info.stop = 1;
	pthread_cond_broadcast(&info.pending_cond);

	for (i = 0; info.worker_thread[i] && i < info.nr_threads; i ++)
		pthread_join(info.worker_thread[i], NULL);

	pthread_cond_destroy(&info.pending_cond);
	pthread_mutex_destroy(&info.pending_lock);
	pthread_mutex_destroy(&info.startup_lock);
	free(info.worker_thread);

	info.stop = 0;

	return 0;
}

int rdwr_submit(struct extend_buf *b)
{
	pthread_mutex_lock(&info.pending_lock);
	list_add_tail(&b->data_list, &info.pending_list);
	pthread_cond_signal(&info.pending_cond);
	pthread_mutex_unlock(&info.pending_lock);

	return 0;
}

static int rdwr_init()
{
	int ret;

	memset(&info, 0, sizeof(struct thread_info));
	ret = rdwr_thread_open(NR_THREAD);

	return ret;
}

static struct ioengine_ops rdwr_engine = {
	.name = "rdwr",
	.io_init = rdwr_init,
	.io_exit = rdwr_exit,
	.io_submit = rdwr_submit,
};

void rdwr_register()
{
	register_ioengine(&rdwr_engine);
}
