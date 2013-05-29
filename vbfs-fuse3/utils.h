#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <malloc.h>
#include <dirent.h>
#include <linux/types.h>
#include <fuse.h>
#include <endian.h>
#include <byteswap.h>
#include <pthread.h>
#include <assert.h>

#include "list.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16_to_cpu(x)  ((__u16)(x))
#define le32_to_cpu(x)  ((__u32)(x))
#define le64_to_cpu(x)  ((__u64)(x))
#define cpu_to_le16(x)  ((__u16)(x))
#define cpu_to_le32(x)  ((__u32)(x))
#define cpu_to_le64(x)  ((__u64)(x))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le16_to_cpu(x)  bswap_16(x)
#define le32_to_cpu(x)  bswap_32(x)
#define le64_to_cpu(x)  bswap_64(x)
#define cpu_to_le16(x)  bswap_16(x)
#define cpu_to_le32(x)  bswap_32(x)
#define cpu_to_le64(x)  bswap_64(x)
#endif

char *pathname_str_sep(char **pathname, const char delim);

int get_lastname(char *pathname, char *last_name, const char delim);

#endif
