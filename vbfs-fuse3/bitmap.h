#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "utils.h"
#include "super.h"
#include "vbfs-fuse.h"

struct vbfs_bitmap {
	size_t max_bit;
	size_t map_len; /* every 4 char size as one */
	__u32 *bitmap;
};

struct inode_bitmap_info {
	__u32 group_no;
	__u32 total_inode;
	__u32 free_inode;
	__u32 current_position;
};

struct extend_bitmap_info {
	__u32 group_no;
	__u32 total_extend;
	__u32 free_extend;
	__u32 current_position;
};

static inline int bitops_ffs(__u32 word)
{
	return ffs(word);
}

static inline int bitops_ffz(__u32 word)
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

/*
 *
 * */
void vbfs_init_bitmap();
int alloc_extend_bitmap(__u32 *extend_no);
int free_extend_bitmap(const __u32 extend_no);
int free_extends(struct inode_vbfs *inode_v);

int alloc_inode_bitmap(__u32 *inode_no);
int free_inode_bitmap(const __u32 inode_no);

#endif
