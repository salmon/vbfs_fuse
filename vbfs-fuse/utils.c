#include "utils.h"
#include "log.h"
#include "super.h"

/*
 * memory operations begin
 * */
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

void *mp_malloc(unsigned int size)
{
	return Malloc(size);
}

void *mp_valloc(unsigned int size)
{
	return Valloc(size);
}

void mp_free(void *p)
{
	free(p);
}

/*
 * file operations begin 
 * */
int write_to_disk(int fd, void *buf, uint64_t offset, size_t len)
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

int read_from_disk(int fd, void *buf, uint64_t offset, size_t len)
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

int write_extend(uint32_t extend_no, void *buf)
{
#if 0
	size_t len = get_extend_size();
	int fd = get_disk_fd();
	off64_t offset = (uint64_t)extend_no * len;

	if (write_to_disk(fd, buf, offset, len))
		return -1;

	return 0;
#endif
	usleep(100000);
	return 0;
}

int read_extend(uint32_t extend_no, void *buf)
{
#if 0
	size_t len = get_extend_size();
	int fd = get_disk_fd();
	off64_t offset = (uint32_t)extend_no * len;

	if (read_from_disk(fd, buf, offset, len))
		return -1;

	return 0;
#endif
	usleep(400000);
	return 0;
}


/*
 * bitmap operations begin
 * */
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#define UNIT_SIZE sizeof(uint32_t)
#define BITS_PER_UNIT (UNIT_SIZE * CHAR_BIT)
#define UNIT_OFFSET(b) ((b) / BITS_PER_UNIT)
#define BIT_OFFSET(b) ((b) % BITS_PER_UNIT)
#define BIT_VALUE(b) ((uint32_t) 1 << BIT_OFFSET(b))

int bitmap_set_bit(struct vbfs_bitmap *bitmap, size_t bit)
{
	uint32_t *bm = NULL;
	uint32_t value = 0;

	if (bitmap->max_bit <= bit)
		return -1;

	bm = bitmap->bitmap;
	bm += UNIT_OFFSET(bit);

	value = le32_to_cpu(*bm);
	value |= BIT_VALUE(bit);
	*bm = le32_to_cpu(value);

	return 0;
}

int bitmap_clear_bit(struct vbfs_bitmap *bitmap, size_t bit)
{
	uint32_t *bm = NULL;
	uint32_t value = 0;

	if (bitmap->max_bit <= bit)
		return -1;

	bm = bitmap->bitmap;
	bm += UNIT_OFFSET(bit);

	value = le32_to_cpu(*bm);
	value &= ~BIT_VALUE(bit);
	*bm = le32_to_cpu(value);

	return 0;
}

static int bitmap_is_set(struct vbfs_bitmap *bitmap, size_t bit)
{
	uint32_t *bm = NULL;
	uint32_t value = 0;

	bm = bitmap->bitmap;
	bm += UNIT_OFFSET(bit);

	value = le32_to_cpu(*bm);

	return !!(value & BIT_VALUE(bit));
}

int bitmap_get_bit(struct vbfs_bitmap *bitmap, size_t bit, int *result)
{
	if (bitmap->max_bit <= bit)
		return -1;

	*result = bitmap_is_set(bitmap, bit);
	return 0;
}

void bitmap_set_all(struct vbfs_bitmap *bitmap)
{
	uint32_t *bm = NULL;
	uint32_t value = 0;
	int tail = bitmap->max_bit % BITS_PER_UNIT;

	memset(bitmap->bitmap, 0xff,
		bitmap->map_len * (BITS_PER_UNIT / CHAR_BIT));

	if (tail) {
		bm = bitmap->bitmap;
		bm += bitmap->map_len - 1;

		value = le32_to_cpu(*bm);
		value &= (uint32_t) -1 >> (BITS_PER_UNIT - tail);
		*bm = cpu_to_le32(value);
	}
}

void bitmap_clear_all(struct vbfs_bitmap *bitmap)
{
	memset(bitmap->bitmap, 0,
		bitmap->map_len * (BITS_PER_UNIT / CHAR_BIT));
}

int bitmap_is_all_set(struct vbfs_bitmap *bitmap)
{
	int i;
	int unused_bits;
	size_t size;

	unused_bits = bitmap->map_len * BITS_PER_UNIT - bitmap->max_bit;

	size = bitmap->map_len;
	if (unused_bits > 0)
		size --;

	for (i = 0; i < size; i ++) {
		if (le32_to_cpu(bitmap->bitmap[i]) != -1)
			return -1;
	}

	if (unused_bits > 0) {
		if ((le32_to_cpu(bitmap->bitmap[size]) &
			(((uint32_t) 1 << (BITS_PER_UNIT - unused_bits)) - 1))
			!= (((uint32_t) 1 << (BITS_PER_UNIT - unused_bits)) - 1)) {
			return -1;
		}
	}

	return 0;
}

/*
 * @pos: the position after which to search for a set bit
 *
 * Returns the position of the found bit, or -1 if no bit found.
 * */
int bitmap_next_set_bit(struct vbfs_bitmap *bitmap, int pos)
{
	size_t nl;
	size_t nb;
	uint32_t bits;

	if (pos < 0)
		pos = -1;

	pos++;

	if (pos >= bitmap->max_bit)
		return -1;

	nl = pos / BITS_PER_UNIT;
	nb = pos % BITS_PER_UNIT;

	bits = le32_to_cpu(bitmap->bitmap[nl]) & ~(((uint32_t) 1 << nb) - 1);

	while (bits == 0 && ++nl < bitmap->map_len) {
		bits = le32_to_cpu(bitmap->bitmap[nl]);
	}

	if (nl == bitmap->map_len - 1) {
		int tail = bitmap->max_bit % BITS_PER_UNIT;

		if (tail)
			bits &= (((uint32_t) 1 << tail) - 1);
	}
	if (bits == 0)
		return -1;

	return bitops_ffs(bits) - 1 + nl * BITS_PER_UNIT;
}

int bitmap_next_clear_bit(struct vbfs_bitmap *bitmap, int pos)
{
	size_t nl;
	size_t nb;
	unsigned long bits;

	if (pos < 0)
		pos = -1;

	pos++;

	if (pos >= bitmap->max_bit)
		return -1;

	nl = pos / BITS_PER_UNIT;
	nb = pos % BITS_PER_UNIT;

	bits = ~(le32_to_cpu(bitmap->bitmap[nl])) & ~(((uint32_t) 1 << nb) - 1);

	while (bits == 0 && ++nl < bitmap->map_len) {
		bits = ~(le32_to_cpu(bitmap->bitmap[nl]));
	}

	if (nl == bitmap->map_len - 1) {
		int tail = bitmap->max_bit % BITS_PER_UNIT;

		if (tail)
			bits &= (uint32_t) -1 >> (BITS_PER_UNIT - tail);
	}
	if (bits == 0)
		return -1;

	return bitops_ffs(bits) - 1 + nl * BITS_PER_UNIT;
}

static int count_one_bits(uint32_t word)
{
	int i, ret = 0;
	int len = BITS_PER_UNIT;

	for (i = 0; i < len; i ++) {
		if (word & (1 << i))
			++ret;
	}

	return 0;
}

int bitmap_count_bits(struct vbfs_bitmap *bitmap)
{
	size_t i, size;
	int ret = 0;
	uint32_t *bm = NULL;
	uint32_t val = 0;
	int unused_bits = 0;

	size = bitmap->map_len;
	bm = bitmap->bitmap;

	unused_bits = bitmap->map_len * BITS_PER_UNIT - bitmap->max_bit;
	if (unused_bits > 0)
		size --;

	for (i = 1; i < size; i ++) {
		val = le32_to_cpu(*bm++);
		ret += count_one_bits(val);
	}

	if (unused_bits > 0) {
		val = *bm;
		for (i = 0; i < unused_bits; i ++) {
			if (val & (1 << i))
				++ret;
		}
	}

	return ret;
}

