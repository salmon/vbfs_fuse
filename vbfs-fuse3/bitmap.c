#include <stdio.h>
#include <string.h>
#include <linux/types.h>

struct vbfs_bitmap {
	size_t max_bit;
	size_t map_len;
	unsigned long *map;
};

int bitmap_set_bit(struct vbfs_bitmap *bitmap, size_t bit);

int bitmap_clear_bit(struct vbfs_bitmap *bitmap, size_t bit);

static bool bitmap_is_set(struct vbfs_bitmap *bitmap, size_t bit);

int bitmap_get_bit(struct vbfs_bitmap *bitmap, size_t bit, bool *result);

void bitmap_set_all(struct vbfs_bitmap *bitmap);

void bitmap_clear_all(struct vbfs_bitmap *bitmap);

bool bitmap_is_all_set(struct vbfs_bitmap *bitmap);

bool bitmap_is_all_clear(struct vbfs_bitmap *bitmap);

int bitmap_next_set_bit(struct vbfs_bitmap *bitmap, int pos);

int bitmap_next_clear_bit(struct vbfs_bitmap *bitmap, int pos);

int bitmap_count_bits(struct vbfs_bitmap *bitmap);
