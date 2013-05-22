#include "log.h"
#include "direct-io.h"

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

int direct_io(struct extend_data *edata)
{
	size_t len = vbfs_ctx.super.s_extend_size;

	pthread_mutex_lock(&edata->ed_lock);

	if (BUFFER_NOT_READY == edata->status) {
		if (NULL == edata->buf)
			edata->buf = mp_valloc(len);
		else
			log_err("something wrong\n");

		memset(edata->buf, 0, len);
		read_extend(edata->extend_no, edata->buf);
	} else if (BUFFER_DIRTY == edata->status) {
		write_extend(edata->extend_no, edata->buf);
	}

	edata->status = BUFFER_CLEAN;

	pthread_cond_signal(&edata->ed_cond);

	pthread_mutex_unlock(&edata->ed_lock);

	return 0;
}
