#ifndef __VBFS_FUSE_H__
#define __VBFS_FUSE_H__

#include "../vbfs_fs.h"
#include "utils.h"
#include "super.h"
#include "extend.h"
#include "bitmap.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#define PATH_SEP '/'

#define INTERNAL_ERR 1
#define DIR_NOT_FOUND 2

enum {
	CLEAN,
	DIRTY,
};

typedef struct {
	int fd;

	struct superblock_vbfs super;

	struct list_head active_inode_list;
	pthread_mutex_t active_inode_lock;

	struct queue *meta_queue;
	struct queue *data_queue;
} vbfs_fuse_context_t;

#endif
