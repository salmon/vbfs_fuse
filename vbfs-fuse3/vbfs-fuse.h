#ifndef __VBFS_FUSE_H__
#define __VBFS_FUSE_H__

#include "../vbfs_fs.h"
#include "utils.h"
#include "super.h"
#include "inode.h"

#define PATH_SEP '/'
#define ROOT_INO 0

#define INTERNAL_ERR 1
#define DIR_NOT_FOUND 2

typedef enum {
	UPDATE_ATIME = 1 << 0,
	UPDATE_MTIME = 1 << 1,
	UPDATE_CTIME = 1 << 2,
} time_update_flags;

typedef struct {
	int fd;

	struct superblock_vbfs super;
	pthread_mutex_t lock_super;

	struct list_head active_inode_list;
	pthread_mutex_t lock_active_inode;
} vbfs_fuse_context_t;

#endif
