#ifndef __VBFS_FUSE_H_
#define __VBFS_FUSE_H_

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
	__u32 s_magic;
	__u32 s_extend_size;
	__u32 s_free_count;
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

	int superblock_vbfs_dirty;
};

struct inode_bitmap_info {
	__u32 group_no;
	__u32 total_inode;
	__u32 free_inodes;
	__u32 current_position;
};

struct extend_bitmap_info {
	__u32 group_no;
	__u32 total_extend;
	__u32 free_extends;
	__u32 current_position;
};

struct extend_content {
	__u32 extend_no;
	char *extend_buf;

	int extend_dirty;
	int nref;
	struct list_head data_list;
};

struct inode_vbfs {
	__u32 i_ino;
	__u32 i_pino;
	__u32 i_mode;
	__u64 i_size;

	__u32 i_ctime;
	__u32 i_atime;
	__u32 i_mtime;

	__u32 i_extend;

	/* store inode metadata */
	char *inode_data;
	int inode_data_dirty;
#if 0
	/* cache write data */
	struct extend_content *write_data_buf;
	/* cache read data */
	struct extend_content *read_data_buf;
#endif
	struct list_head data_buf_list;

	int nref;
	struct list_head inode;
	pthread_mutex_t lock_inode;
};

struct dentry_info {
	__u32 group_no;
	__u32 used_extend_no;

	__u32 dir_self_count;
	__u32 dir_total_count;
	__u32 next_extend;
	__u32 dentry_bitmap_len;

	char *dentry_bitmap;
};

/* */
struct dentry_vbfs {
	__u32 inode;
	__u32 file_type;

	char name[NAMELEN];
	struct list_head dentry_list;
};

/* 
 * If someone want to operate extend_bitmap_cache,
 * it must take lock_ext_bm_cache, so only one operate
 * it once time so ref always 1.
 * */
struct extend_bitmap_cache {
	__u32 extend_no; /* record current extend bitmap cached */

	char *content;
	struct extend_bitmap_info extend_bm_info;
	char *extend_bitmap_region;

	int cache_status; /* 0->(not ready) 1->(ready) */

	int extend_bitmap_dirty;

	pthread_mutex_t lock_ext_bm_cache;
};

struct inode_bitmap_cache {
	__u32 extend_no; /* record current inode bitmap cached */

	char *content;
	struct inode_bitmap_info inode_bm_info;
	char *inode_bitmap_region;

	int cache_status; /* 0->(not ready) 1->(ready) */

	int inode_bitmap_dirty;

	pthread_mutex_t lock_ino_bm_cache;
};

struct inode_cache_in_ext {
	__u32 extend_no; /* record current the extend of inodes cached */
	int cache_no; /* according this to swap out */

	char *content;

	int cache_status; /* 0->(not ready) 1->(ready) */
	int inode_cache_dirty;

	struct list_head ino_cache_in_ext_list;
	pthread_mutex_t lock_inode_cache;
};

typedef struct {
	int fd;

	struct superblock_vbfs *super;
	pthread_mutex_t lock_super;

	struct extend_bitmap_cache *extend_bitmap_array;

	struct inode_bitmap_cache *inode_bitmap_array;

	struct list_head *inode_cache_list;

	struct list_head active_inode_list;
	pthread_mutex_t lock_inode_list;
} vbfs_fuse_context_t;

#endif
