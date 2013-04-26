#ifndef __UTILS_H_
#define __UTILS_H_

#include <malloc.h>
#include <linux/types.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include "log.h"

void *Valloc(unsigned int size);

int write_to_disk(int fd, void *buf, __u64 offset, size_t len);

int read_from_disk(int fd, void *buf, __u64 offset, size_t len);

#endif
