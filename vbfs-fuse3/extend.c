#include "log.h"
#include "mempool.h"
#include "vbfs-fuse.h"

extern vbfs_fuse_context_t vbfs_ctx;

int write_to_disk(int fd, void *buf, __u64 offset, size_t len)
{
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		log_err("lseek error %s\n", strerror(errno));
		return -1;
	}
	if (write(fd, buf, len) < 0) {
		log_err("write error %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int read_from_disk(int fd, void *buf, __u64 offset, size_t len)
{
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		log_err("lseek error %s\n", strerror(errno));
		return -1;
	}
	if (read(fd, buf, len) < 0) {
		log_err("read error %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static int write_extend(__u32 extend_no, void *buf)
{
	size_t len = vbfs_ctx.super.s_extend_size;
	int fd = vbfs_ctx.fd;
	off64_t offset = (__u64)extend_no * len;

	if (write_to_disk(fd, buf, offset, len))
		return -1;

	return 0;
}

static int read_extend(__u32 extend_no, void *buf)
{
	size_t len = vbfs_ctx.super.s_extend_size;
	int fd = vbfs_ctx.fd;
	off64_t offset = (__u64)extend_no * len;

	if (read_from_disk(fd, buf, offset, len))
		return -1;

	return 0;
}

static struct extend_data *open_edata_unlocked(const __u32 extend_no, \
					struct extend_queue *equeue, int *ret)
{
	struct extend_data *edata = NULL;

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
	edata->inode_ref = 0;

	pthread_mutex_init(&edata->ed_lock, NULL);
	INIT_LIST_HEAD(&edata->data_list);
	INIT_LIST_HEAD(&edata->rq_list);

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
	int ret = 0;

/*
 * may replace by
 * equeue->personality_close(edata)
 * */
	equeue = edata->equeue;

	pthread_mutex_lock(&equeue->all_ed_lock);
	pthread_mutex_lock(&edata->ed_lock);

	if (--edata->ref > 0) {
		pthread_mutex_unlock(&edata->ed_lock);
		pthread_mutex_unlock(&equeue->all_ed_lock);
		return 0;
	}

	list_del(&edata->rq_list);

	if (BUFFER_DIRTY == edata->status) {
		log_dbg("write extend %u", edata->extend_no);
		if(write_extend(edata->extend_no, edata->buf))
			ret = -EIO;
	}
	pthread_mutex_unlock(&edata->ed_lock);
	pthread_mutex_unlock(&equeue->all_ed_lock);

	pthread_mutex_destroy(&edata->ed_lock);
	if (BUFFER_NOT_READY != edata->status)
		free(edata->buf);
	free(edata);

	return ret;
}

int read_edata(struct extend_data *edata)
{
	int ret = 0;
	size_t len = vbfs_ctx.super.s_extend_size;

	pthread_mutex_lock(&edata->ed_lock);

	if (BUFFER_NOT_READY == edata->status) {
		edata->buf = mp_valloc(len);
		if (NULL == edata->buf) {
			pthread_mutex_unlock(&edata->ed_lock);
			return -ENOMEM;
		}

		ret = read_extend(edata->extend_no, edata->buf);
		if (0 == ret) {
			edata->status = BUFFER_CLEAN;
		} else {
			mp_free(edata->buf);
			edata->buf = NULL;
			ret = -EIO;
		}
	}

	pthread_mutex_unlock(&edata->ed_lock);

	return ret;
}

int sync_edata(struct extend_data *edata)
{
	int ret = 0;

	pthread_mutex_lock(&edata->ed_lock);

	if (BUFFER_DIRTY == edata->status) {
		ret = write_extend(edata->extend_no, edata->buf);
		if (0 == ret) {
			edata->status = BUFFER_CLEAN;
		} else
			ret = -EIO;
	}

	pthread_mutex_unlock(&edata->ed_lock);

	return ret;
}
