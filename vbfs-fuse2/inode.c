#include "utils.h"

static int alloc_extend_bitmap(__u32 *extend_no)
{
	return 0;
}

static int alloc_inode_bitmap(__u32 *inode_no)
{
	return 0;
}

static int free_extend_bitmap()
{
	return 0;
}

static int free_inode_bitmap()
{
	return 0;
}

struct inode_vbfs *vbfs_inode_open(__u32 ino, int *err_no)
{
	return NULL;
}

struct inode_vbfs *vbfs_inode_create(__u32 ino, __u32 mode_t, int *err_no)
{
	return NULL;
}

int vbfs_inode_sync(struct inode_vbfs *i_vbfs)
{
	return 0;
}

int vbfs_inode_close(struct inode_vbfs *i_vbfs)
{
	return 0;
}

int vbfs_inode_mark_dirty(struct inode_vbfs *i_vbfs)
{
	return 0;
}
