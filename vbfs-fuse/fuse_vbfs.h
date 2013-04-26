#ifndef __FUSE_VBFS_H_
#define __FUSE_VBFS_H_

#include <sys/types.h>
#include <linux/types.h>

#include "../vbfs_format.h"
#include "utils.h"

#define PATH_SEP '/'
#define ROOT_INO 0

#define INTERNAL_ERR 1
#define DIR_NOT_FOUND 2

typedef enum {
	UPDATE_ATIME = 1 << 0,
	UPDATE_MTIME = 1 << 1,
	UPDATE_CTIME = 1 << 2,
} time_update_flags;

struct superblock_vbfs {
	int fd;

	__u32 s_magic;
	__u32 s_extend_size;
	__u32 s_free_count;
	__u32 s_inode_count;

	__u32 bad_count;
	__u32 bad_extend_count;
	__u32 bad_extend_offset;
	__u32 bad_extend_current;
	__u32 *bad_blocks;
	int bad_dirty;

	__u32 extend_bitmap_count;
	__u32 extend_bitmap_current;
	__u32 extend_bitmap_offset;
	char *extend_bitmap;
	int extend_bitmap_dirty;

	__u32 inode_bitmap_count;
	__u32 inode_bitmap_offset;
	__u32 inode_bitmap_current;
	char *inode_bitmap;
	int inode_bitmap_dirty;

	__u32 inode_extend_count;

	__u32 s_ctime;
	__u32 s_mount_time;
	__u32 s_state; /* clean or dirty */

	__u8 uuid[16];
};

struct inode_vbfs {
	__u32 i_ino;
	__u32 i_pino;
	__u16 i_mode;
	__u16 i_size;

	__u32 i_atime;
	__u32 i_ctime;
	__u32 i_mtime;

	__u32 i_extends;
};

struct dentry_vbfs {
	__u32 inode;
	__u8 file_type;
	__u16 rec_len;
	char name[NAME_LEN];

	int dirty;
};

struct inode_bitmap_info {
	__u32 group_no;
	__u32 total_inode;
	__u32 free_inode;
	__u32 current_position;
	__u64 inode_start_offset;
};

struct extend_bitmap_info {
	__u32 group_no;
	__u32 total_extend;
	__u32 free_extend;
	__u32 current_position;
	__u64 extend_start_offset;
};

struct dir_info {
	__u32 dir_count;
	__u32 start_count;
	__u32 next_extend;
};

typedef struct {
	struct superblock_vbfs *vbfs_super;

	struct extend_bitmap_info *extend_info;
	char *extend_bitmap_region;
	int extend_bitmap_dirty;

	struct inode_bitmap_info *inode_info;
	char *inode_bitmap_region;
	int inode_bitmap_dirty;

	off64_t inode_offset;
	__u32 inode_count_per_extend;

	__u32 inode_extend_count;
	off64_t extend_offset;

	char *inode_cache;
	__u32 inode_cache_extend;
	__u32 inode_cache_line_off;
	int inode_dirty;
} vbfs_fuse_context_t; 

#endif
