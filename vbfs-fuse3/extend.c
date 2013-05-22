#include "extend.h"
#include "io-thread.h"

static struct extend_data *open_edata_unlocked(const __u32 extend_no, \
					struct extend_queue *equeue, int *ret)
{
	struct extend_data *edata = NULL;

	printf("open extend_no %d\n", extend_no);
	list_for_each_entry(edata, &equeue->all_ed_list, rq_list) {
		if (extend_no == edata->extend_no) {
			pthread_mutex_lock(&edata->ed_lock);
			edata->ref ++;
			pthread_mutex_unlock(&edata->ed_lock);
			return edata;
		}
	}

	edata = malloc(sizeof(struct extend_data));
	if (NULL == edata) {
		*ret = -ENOMEM;
		return NULL;
	}

	edata->extend_no = extend_no;
	edata->buf = NULL;

	edata->status = BUFFER_NOT_READY;
	edata->ref = 1;
	edata->flags = 0;
	edata->err_no = 0;

	pthread_mutex_init(&edata->ed_lock, NULL);
	pthread_cond_init(&edata->ed_cond, NULL);

	INIT_LIST_HEAD(&edata->data_list);
	edata->equeue = equeue;

	list_add(&edata->rq_list, &equeue->all_ed_list);

	return edata;
}

struct extend_data *open_edata(const __u32 extend_no, \
			struct extend_queue *equeue, int *ret)
{
	struct extend_data *edata = NULL;

	pthread_mutex_lock(&equeue->all_ed_lock);
	edata = open_edata_unlocked(extend_no, equeue, ret);
	pthread_mutex_unlock(&equeue->all_ed_lock);

	return edata;
}

int close_edata(struct extend_data *edata)
{
	struct extend_queue *equeue = NULL;

	printf("close extend_no %d, ref %d\n", edata->extend_no, edata->ref);
	equeue = edata->equeue;
/*
 * may replace by
 * equeue->personality_close(edata)
 * */

	pthread_mutex_lock(&equeue->all_ed_lock);
	pthread_mutex_lock(&edata->ed_lock);

	if (--edata->ref > 0) {
		pthread_mutex_unlock(&edata->ed_lock);
		pthread_mutex_unlock(&equeue->all_ed_lock);
		return 0;
	}


	fprintf(stderr, "free edata %p extend_no %u\n", edata, edata->extend_no);

	list_del(&edata->rq_list);

	pthread_mutex_unlock(&edata->ed_lock);
	pthread_mutex_unlock(&equeue->all_ed_lock);

	pthread_mutex_destroy(&edata->ed_lock);
	pthread_cond_destroy(&edata->ed_cond);
	free(edata->buf);
	free(edata);

/**************/

	return 0;
}

int read_edata(struct extend_data *edata)
{
	pthread_mutex_lock(&edata->ed_lock);

	while (BUFFER_NOT_READY == edata->status) {
		extend_submit(edata);
		pthread_cond_wait(&edata->ed_cond, &edata->ed_lock);
	}

	pthread_mutex_unlock(&edata->ed_lock);

	return 0;
}

int write_edata(struct extend_data *edata)
{
	pthread_mutex_lock(&edata->ed_lock);

	while (BUFFER_DIRTY == edata->status) {
		extend_submit(edata);
		pthread_cond_wait(&edata->ed_cond, &edata->ed_lock);
	}

	pthread_mutex_unlock(&edata->ed_lock);

	return 0;
}
