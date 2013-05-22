#include "utils.h"
#include "vbfs-fuse.h"
#include "log.h"

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

__u32 bitops_next_pos_set(char *bitmap, __u32 bitmap_bits, __u32 start_ops)
{
	__u32 offset = 0;
	__u32 val;
	__u32 *bm = NULL;
	__u32 bitmap_size = 0;
	int n = 0, i = 0, len = 0;
	int ret = 0;

	assert(! (bitmap_bits % 32));

	bitmap_size = bitmap_bits / 32;
	bm = (__u32 *) bitmap;

	offset = start_ops / 32;
	n = start_ops % 32;

	len = bitmap_size - offset;
	if (len <= 0)
		return 0;

	val = le32_to_cpu(*(bm + offset)) >> n;

	for (i = 0; i < len; i ++) {
		ret = bitops_ffs(val);
		if (ret) {
			return offset * 32 + n + ret;
		} else {
			val = *++bm;
		}
	}

	return 0;
}

__u32 bitops_next_pos_zero(char *bitmap, __u32 bitmap_bits, __u32 start_ops)
{

	__u32 offset = 0;
	__u32 val = 0;
	__u32 *bm = NULL;
	__u32 bitmap_size = 0;
	int n = 0, i = 0, len = 0;
	int ret = 0;

	assert(! (bitmap_bits % 32));

	bitmap_size = bitmap_bits / 32;
	bm = (__u32 *) bitmap;

	offset = start_ops / 32;
	n = start_ops % 32;

	len = bitmap_size - offset;
	if (len <= 0)
		return 0;

	val = le32_to_cpu(*(bm + offset)) >> n;

	for (i = 0; i < len; i ++) {
		ret = bitops_ffz(val);
		if (ret) {
			return offset * 32 + n + ret;
		} else {
			val = *++bm;
		}
	}

	return 0;
}

__u32 find_zerobit_and_set(char *bitmap, __u32 bitmap_bits, __u32 start_ops)
{
	__u32 offset = 0;
	__u32 val = 0;
	__u32 *bm = NULL;
	int m = 0, n = 0;

	offset = bitops_next_pos_zero(bitmap, bitmap_bits, start_ops);
	if (offset == 0) {
		return offset;
	}

	offset --;
	m = offset / 32;
	n = offset % 32;

	bm = (__u32 *) bitmap;

	val = le32_to_cpu(*(bm + m));
	*(bm + m) = cpu_to_le32(val | 1 << n);

	return ++offset;
}

char *pathname_str_sep(char **pathname, const char delim)
{
	char *sbegin = *pathname;
	char *end;
	char *sc;
	int found = 0;

	if (sbegin == NULL)
		return NULL;

	for (sc = sbegin; *sc != '\0'; ++sc) {
		if (*sc == delim) {
			found = 1;
			end = sc;
			break;
		}
	}

	if (! found)
		end = NULL;

	if (end)
		*end++ = '\0';

	*pathname = end;

	return sbegin;
}

int get_lastname(char *pathname, char *last_name, const char delim)
{
	int len = 0;
	char *pos = NULL;

	if (pathname == NULL) {
		return -1;
	}

	len = strlen(pathname);

	while (pathname[--len] == delim) {
		if (len <= 1)
			return 0;

		pathname[len] = '\0';
	}

	for (; len >= 0; len --) {
		if (pathname[len] == delim) {
			pos = &pathname[len + 1];
			strncpy(last_name, pos, NAME_LEN);

			pathname[len + 1] = '\0';
			break;
		}
	}


	return 0;
}
