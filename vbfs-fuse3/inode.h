#ifndef __INODE_H__
#define __INODE_H__

#include "utils.h"

#define ROOT_INO 0

enum {
	INODE_CLEAN,
	INODE_DIRTY,
};

typedef enum {
	UPDATE_ATIME = 1 << 0,
	UPDATE_MTIME = 1 << 1,
	UPDATE_CTIME = 1 << 2,
} time_update_flags;

struct inode_vbfs {
	__u32 i_ino;
	__u32 i_pino;
	__u32 i_mode;
	__u64 i_size;

	__u32 i_ctime;
	__u32 i_atime;
	__u32 i_mtime;

	__u32 i_extend;

#if 0
	/* store inode first extend */
	char *inode_first_ext;
	int first_ext_status; /* 0 not in buf, 1 clean, 2 dirty*/
#endif

	int inode_dirty;

	int ref;
	struct list_head active_list;
	pthread_mutex_t inode_lock;

	struct list_head data_buf_list;
};

int init_root_inode();

struct inode_vbfs *get_root_inode();

struct inode_vbfs *vbfs_inode_open(__u32 ino, int *err_no);

struct inode_vbfs *alloc_inode(__u32 p_ino, __u32 mode_t, int *err_no);
//struct inode_vbfs *vbfs_inode_create(__u32 p_ino, __u32 mode_t, int *err_no);

int vbfs_inode_sync(struct inode_vbfs *inode_v);

int vbfs_inode_close(struct inode_vbfs *inode_v);

int vbfs_inode_update_times(struct inode_vbfs *v_inode, time_update_flags mask);

int vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode_parent, const char *name, __u32 *ino);

struct inode_vbfs *vbfs_pathname_to_inode(const char *pathname, int *err_no);

#endif
