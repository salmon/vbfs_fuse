#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16_to_cpu(x)  ((uint16_t)(x))
#define le32_to_cpu(x)  ((uint32_t)(x))
#define le64_to_cpu(x)  ((uint64_t)(x))
#define cpu_to_le16(x)  ((uint16_t)(x))
#define cpu_to_le32(x)  ((uint32_t)(x))
#define cpu_to_le64(x)  ((uint64_t)(x))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le16_to_cpu(x)  bswap_16(x)
#define le32_to_cpu(x)  bswap_32(x)
#define le64_to_cpu(x)  bswap_64(x)
#define cpu_to_le16(x)  bswap_16(x)
#define cpu_to_le32(x)  bswap_32(x)
#define cpu_to_le64(x)  bswap_64(x)
#endif

#define BUG_ON(x) assert(!(x))

static inline int test_bit(int bit, unsigned int *val)
{
	return *val & (1 << bit);
}

static inline void set_bit(int bit, unsigned int *val)
{
	*val |= (1 << bit);
}

static inline void clear_bit(int bit, unsigned int *val)
{
	*val &= ~(1 << bit);
}

static inline int test_and_set_bit(int bit, unsigned int *val)
{
	unsigned int old = *val;
	*val |= (1 << bit);

	return old & (1 << bit);
}

static inline unsigned long get_curtime()
{
	struct timeval curtime;

	gettimeofday(&curtime, NULL);

	return curtime.tv_sec;
}

void *Valloc(unsigned int size);
void *Malloc(unsigned int size);

void *mp_malloc(unsigned int size);
void *mp_valloc(unsigned int size);
void mp_free(void *p);


int write_to_disk(int fd, void *buf, uint64_t offset, size_t len);
int read_from_disk(int fd, void *buf, uint64_t offset, size_t len);
int write_extend(uint32_t extend_no, void *buf);
int read_extend(uint32_t extend_no, void *buf);


struct vbfs_bitmap {
	size_t max_bit;
	size_t map_len; /* every 4 char size as one */
	uint32_t *bitmap;
};

static inline int bitops_ffs(uint32_t word)
{
	return ffs(word);
}

static inline int bitops_ffz(uint32_t word)
{
	return bitops_ffs(~word);
}

int bitmap_set_bit(struct vbfs_bitmap *bitmap, size_t bit);
int bitmap_clear_bit(struct vbfs_bitmap *bitmap, size_t bit);
int bitmap_get_bit(struct vbfs_bitmap *bitmap, size_t bit, int *result);
void bitmap_set_all(struct vbfs_bitmap *bitmap);
void bitmap_clear_all(struct vbfs_bitmap *bitmap);
int bitmap_is_all_set(struct vbfs_bitmap *bitmap);
int bitmap_next_set_bit(struct vbfs_bitmap *bitmap, int pos);
int bitmap_next_clear_bit(struct vbfs_bitmap *bitmap, int pos);
int bitmap_count_bits(struct vbfs_bitmap *bitmap);

#endif
