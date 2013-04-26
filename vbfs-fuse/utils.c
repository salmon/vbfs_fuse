#include "utils.h"
#include "log.h"

void *Valloc(unsigned int size)
{
	void *p = NULL;
	if ((p = valloc(size)) != NULL)
		return p;
	else {
		log_err(pLog, "no memory alloc\n");
		return NULL;
		//exit(1);
	}
}

int write_to_disk(int fd, void *buf, __u64 offset, size_t len)
{
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		log_err(pLog, "lseek error, offset %llu %s\n", offset, strerror(errno));
		return -1;
	}

	if (write(fd, buf, len) < 0) {
		log_err(pLog, "write error %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int read_from_disk(int fd, void *buf, __u64 offset, size_t len)
{
	//log_err(pLog, "offset %llu\n", offset);
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		log_err(pLog, "lseek error, offset %llu %s\n", offset, strerror(errno));
		return -1;
	}

	if (read(fd, buf, len) < 0) {
		log_err(pLog, "read error %s\n", strerror(errno));
		return -1;
	}

	return 0;
}
