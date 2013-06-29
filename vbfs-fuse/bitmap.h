#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "utils.h"

struct bitmap_header {
	uint32_t group_no;
	uint32_t total_cnt;
	uint32_t free_cnt;
	uint32_t current_position;
};

void init_bitmap(struct vbfs_bitmap *bitmap, uint32_t total_bits);

int alloc_extend_bitmap(uint32_t *extend_no);
int free_extend_bitmap(const uint32_t extend_no);
int free_extend_bitmap_async(const uint32_t extend_no);
//int free_extends(struct inode_info *inode);

#endif
