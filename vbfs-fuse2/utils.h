#ifndef __UTILS_H_
#define __UTILS_H_

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

int write_to_disk(int fd, void *buf, __u64 offset, size_t len);
int read_from_disk(int fd, void *buf, __u64 offset, size_t len);

int write_extend(__u32 extend_no, void *buf);
int read_extend(__u32 extend_no, void *buf);

void *Valloc(unsigned int size);
void *Malloc(unsigned int size);

static inline int bitops_ffs(__u32 word)
{
	int r = 0;

	if (!(word & 0xffff)) {
		word >>= 16;
		r += 16;
	}
	if (!(word & 0xff)) {
		word >>= 8;
		r += 8;
	}
	if (!(word & 0xf)) {
		word >>= 4;
		r += 4;
	}
	if (!(word & 3)) {
		word >>= 2;
		r += 2;
	}
	if (!(word & 1)) {
		word >>= 1;
		r += 1;
	}

	return (r + 1) % 32;
}

static inline int bitops_ffz(__u32 word)
{
	return bitops_ffs(~word);
}

int check_ffs(char *bitmap, __u32 bitmap_bits, __u32 bit);

char *pathname_str_sep(char **pathname, const char delim);

__u32 bitops_next_pos_set(char *bitmap, __u32 bitmap_bits, __u32 start_ops);

__u32 bitops_next_pos_zero(char *bitmap, __u32 bitmap_bits, __u32 start_ops);

__u32 find_zerobit_and_set(char *bitmap, __u32 bitmap_bits, __u32 start_ops);

int get_lastname(char *pathname, char *last_name, const char delim);

#endif
