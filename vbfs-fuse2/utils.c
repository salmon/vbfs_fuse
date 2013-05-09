#include "utils.h"
#include "vbfs-fuse.h"
#include "log.h"

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

int write_extend(__u32 extend_no, void *buf)
{
	size_t len = vbfs_ctx.super.s_extend_size;
	int fd = vbfs_ctx.fd;
	off64_t offset = (__u64)extend_no * len;

	if (write_to_disk(fd, buf, offset, len))
		return -1;

	return 0;
}

int read_extend(__u32 extend_no, void *buf)
{
	size_t len = vbfs_ctx.super.s_extend_size;
	int fd = vbfs_ctx.fd;
	off64_t offset = (__u64)extend_no * len;

	if (read_from_disk(fd, buf, offset, len))
		return -1;

	return 0;
}

void *Valloc(unsigned int size)
{
	void *p = NULL;

	if ((p = valloc(size)) != NULL) {
		return p;
	} else {
		log_err("valloc error, no memory\n");
		return NULL;
	}
}

void *Malloc(unsigned int size)
{
	void *p = NULL;

	if ((p = malloc(size)) != NULL) {
		return p;
	} else {
		log_err("malloc error, no memory\n");
		return NULL;
	}
}

int check_ffs(char *bitmap, __u32 bitmap_bits, __u32 bit)
{
	__u32 val = 0;
	__u32 *bm = NULL;
	__u32 offset = 0;

	assert(! (bitmap_bits % 32));
	if (bit > bitmap_bits) {
		return 0;
	}

	bm = (__u32 *) bitmap;
	offset = bit / 32;

	val = le32_to_cpu(*(bm + offset));

	return val | 1 << (bit % 32);
}
