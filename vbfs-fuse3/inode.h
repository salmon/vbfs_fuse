#ifndef __INODE_H__
#define __INODE_H__

#include "utils.h"

struct inode_vbfs {
	__u32 i_ino;
	__u32 i_pino;
	__u32 i_mode;
	__u64 i_size;

	__u32 i_ctime;
	__u32 i_atime;
	__u32 i_mtime;

	__u32 i_extend;

	/* store inode first extend */
	char *inode_first_ext;
	int first_ext_status; /* 0 not in buf, 1 clean, 2 dirty*/

	int inode_dirty;

	struct list_head data_buf_list;

	int ref;
	struct list_head inode_l;
	pthread_mutex_t lock_inode;
};

int init_root_inode();

#endif
