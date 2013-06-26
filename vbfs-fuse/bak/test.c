#include "err.h"
#include "utils.h"
#include "extend.h"
#include "super.h"
#include "ioengine.h"

static struct queue *bm_queue;
static struct queue *data_queue;
static int rand_seed;

int init_queue()
{
	int ret;

	bm_queue = queue_create(20, 4);
	if (IS_ERR(bm_queue)) {
		ret = PTR_ERR(bm_queue);
		printf("bitmap queue create error, %s\n", strerror(ret));
		return ret;
	}

	data_queue = queue_create(128, 6); 
	if (IS_ERR(data_queue)) {
		ret = PTR_ERR(data_queue);
		printf("bitmap queue create error, %s\n", strerror(ret));
		return ret;
	}

	return 0;
}

void *read_thread(void *args)
{
	struct extend_buf *b;
	uint32_t extend_no;
	char *buf;

	while (1) {
		srand(time(NULL) + rand_seed);
		extend_no = rand() % 204800000;

		if (rand_seed % 3)
			buf = extend_new(data_queue, extend_no, &b);
		else
			buf = extend_read(bm_queue, extend_no, &b);

		if (NULL == b || IS_ERR(buf)) {
			printf("read error %s\n", strerror(-PTR_ERR(buf)));
			rand_seed ++;
			continue;
		}
		printf("read correct, %d\n", extend_no);
		buf[1024] = 'a';

		if (rand_seed % 4)
			extend_put(b);
		else
			extend_release(b);

		rand_seed ++;
	}
}

void *write_thread(void *args)
{
	struct extend_buf *b;
	uint32_t extend_no;
	char *buf;

	while (1) {
		srand(time(NULL) + rand_seed);
		extend_no = rand() % 204800000;

		if (rand_seed % 4)
			buf = extend_read(data_queue, extend_no, &b);
		else
			buf = extend_new(bm_queue, extend_no, &b);

		if (NULL == b || IS_ERR(buf)) {
			printf("write error %s\n", strerror(-PTR_ERR(buf)));
			rand_seed ++;
			continue;
		}
		printf("write correct, %d\n", extend_no);
		buf[2048] = 'a';

		extend_mark_dirty(b);

		if (rand_seed % 3)
			extend_write_dirty(b);

		if (rand_seed % 3)
			extend_put(b);
		else
			extend_release(b);

		rand_seed ++;
	}
}

static int init_rdwr()
{
	int ret;

	rdwr_register();
	ret = ioengine->io_init();

	return ret;
}

#define READ_THREAD 3
#define WRITE_THREAD 2

int main()
{
	int ret;
	int i;
	pthread_t tid[5];

	ret = init_queue();
	if (ret)
		return ret;

	ret = init_rdwr();
	if (ret)
		return ret;

	for (i = 0; i < READ_THREAD; i ++) {
		ret = pthread_create(&tid[i], NULL, read_thread, NULL);
	}

	for (i = 0; i < WRITE_THREAD; i ++) {
		ret = pthread_create(&tid[READ_THREAD + i], NULL, write_thread, NULL);
	}

	for (i = 0; i < READ_THREAD + WRITE_THREAD; i ++) {
		pthread_join(tid[i], NULL);
	}

	return 0;
}
