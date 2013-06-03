#ifndef __SUPER_H__
#define __SUPER_H__

#include "utils.h"

enum {
	MOUNT_CLEAN,
	MOUNT_DIRTY,
};

enum {
	SUPER_CLEAN,
	SUPER_DIRTY,
};

struct superblock_vbfs {
	__u32 s_magic;
	__u32 s_extend_size;
	__u32 s_extend_count;
	__u32 s_file_idx_len;
	__u32 s_inode_count;

	__u32 bad_count;
	__u32 bad_extend_count;
	__u32 bad_extend_offset;
	__u32 bad_extend_current;

	__u32 extend_bitmap_count;
	__u32 extend_bitmap_offset;
	__u32 extend_bitmap_current;

	__u32 inode_bitmap_count;
	__u32 inode_bitmap_offset;
	__u32 inode_bitmap_current;

	__u32 s_ctime;
	__u32 s_mount_time;
	__u32 s_state;

	__u8 uuid[16];

	__u32 inode_offset;
	__u32 inode_extends;

	int super_vbfs_dirty;
	__u32 s_free_count;
};

int init_super(const char *dev_name);

int sync_super(void);

const size_t get_extend_size();

__u32 get_extend_bm_curr();
__u32 get_inode_bm_curr();
__u32 add_extend_bm_curr();
__u32 add_inode_bm_curr();
__u32 get_file_idx_size();
__u32 get_file_max_index();

#endif
