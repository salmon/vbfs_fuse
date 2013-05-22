#include "io-thread.h"
#include "direct-io.h"

#include <time.h>

extern struct extend_queue extend_bm_eq;
extern struct extend_queue inode_bm_eq;
extern struct extend_queue inode_eq;
extern struct extend_queue data_eq;

void *read_worker(void *args)
{
	struct extend_data *edata = NULL;
	int extend_no;
	int ret = 0;
	unsigned int seed = time(NULL);

	while (1) {
		extend_no = rand_r(&seed) % 10;
		edata = open_edata(extend_no, &extend_bm_eq, &ret);
		if (ret) {
			printf("open error\n");
			continue;
		}

		ret = read_edata(edata);
		if (ret) {
			printf("read error\n");
			close_edata(edata);
			continue;
		} else
			printf("extend_no %d edata %p edata->buf %p buf[0] %c\n", 
				extend_no , edata, edata->buf, edata->buf[0]);

		//sleep(extend_no % 3);

		close_edata(edata);
	}
}

void *write_worker(void *args)
{
	struct extend_data *edata = NULL;
	int extend_no;
	int ret = 0;
	unsigned int seed = time(NULL);

	while (1) {
		extend_no = rand_r(&seed) % 10;
		edata = open_edata(extend_no, &extend_bm_eq, &ret);
		if (ret)
			printf("open error\n");

		pthread_mutex_lock(&edata->ed_lock);

		if (BUFFER_NOT_READY == edata->status) {
			edata->buf = valloc(1024);
			edata->status = BUFFER_DIRTY;
		}
		memset(edata->buf, 'c', 1024);

		pthread_mutex_unlock(&edata->ed_lock);

		ret = write_edata(edata);
		if (ret) {
			printf("write error\n");
			close_edata(edata);
			continue;
		} else
			printf("extend_no %d edata %p edata->buf %p buf[0] %c\n",
				extend_no, edata, edata->buf, edata->buf[0]);

		//sleep(extend_no % 3);

		close_edata(edata);
	}
}

int main()
{
	pthread_t thread_num[10];
	int i, j;

	rw_thread_init();
	rw_thread_open(direct_io, 8);

	j = 5;
/*
	for (i = 0; i < j; i ++)
		pthread_create(&thread_num[i], NULL, read_worker, NULL);

	for (i = 0; i < (10 - j); i ++)
		pthread_create(&thread_num[i], NULL, write_worker, NULL);
*/
	for (i = 0; i < 10; i ++)
		pthread_create(&thread_num[i], NULL, write_worker, NULL);

	for (i = 0; i < 10; i ++)
		pthread_join(thread_num[i], NULL);

	return 0;
}
