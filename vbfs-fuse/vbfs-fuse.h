#ifndef __VBFS_FUSE_H__
#define __VBFS_FUSE_H__

#include "../vbfs_fs.h"
#include "utils.h"
#include "super.h"
#include "extend.h"
#include "bitmap.h"
#include "dir.h"

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#define PATH_SEP '/'

#define INTERNAL_ERR 1
#define DIR_NOT_FOUND 2
#define INODE_HASH_BITS 8
#define INODE_HASH(ino) \
	((((ino) >> INODE_HASH_BITS) ^ (ino)) & \
	((1 << INODE_HASH_BITS) - 1))
#define BM_RESERVED_MAX 8
#define DATA_RESERVED_MAX 128

enum {
	CLEAN,
	DIRTY,
};

struct active_inode {
	struct hlist_head *inode_cache;
	struct list_head inode_list;

	pthread_mutex_t lock;
};

typedef struct {
	int fd;

	struct active_inode active_i;
	struct superblock_vbfs super;

	struct queue *meta_queue;
	struct queue *data_queue;
} vbfs_fuse_context_t;

#endif
