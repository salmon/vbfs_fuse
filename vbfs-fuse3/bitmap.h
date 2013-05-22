#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "utils.h"
#include "super.h"
#include "vbfs-fuse.h"

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

int alloc_extend_bitmap(__u32 *extend_no);
int free_extend_bitmap(const __u32 extend_no);
int free_extends(struct inode_vbfs *inode_v);

int alloc_inode_bitmap(__u32 *inode_no);
int free_inode_bitmap(const __u32 inode_no);

#endif
