#ifndef __DIRECT_IO_H__
#define __DIRECT_IO_H__

#include "super.h"
#include "vbfs-fuse.h"
#include "extend.h"

int write_to_disk(int fd, void *buf, __u64 offset, size_t len);

int read_from_disk(int fd, void *buf, __u64 offset, size_t len);

int direct_io(struct extend_data *edata);

#endif

