#ifndef __INODE_H__
#define __INODE_H__

#include "utils.h"
#include "list.h"

typedef enum {
	UPDATE_ATIME = 1 << 0,
	UPDATE_MTIME = 1 << 1,
	UPDATE_CTIME = 1 << 2,
} time_update_flags;

enum {
	INODE_REMOVE = 1 << 0,
};

struct vbfs_dirent_header {
	uint32_t group_no;
	uint32_t total_extends; /* useless */

	uint32_t dir_self_count;
	uint32_t dir_total_count; /* useless */
	uint32_t next_extend;
	uint32_t dir_capacity;
	uint32_t bitmap_size;
};

struct vbfs_dirent {
	uint32_t i_ino;
	uint32_t i_pino;

	uint32_t i_mode;
	uint64_t i_size;
	uint32_t i_atime;
	uint32_t i_ctime;
	uint32_t i_mtime;

	char name[NAME_LEN];
};

struct inode_info {
	struct vbfs_dirent *dirent;
	uint32_t data_no;
	uint32_t position;

	int status;
	unsigned int flags;
	int ref;
	pthread_mutex_t lock;

	struct hlist_node hash_list;
	struct list_head active_list;
	struct list_head extend_list;
};

int init_root_inode(void);
struct inode_vbfs *vbfs_inode_open(uint32_t ino);
//struct inode_vbfs *alloc_inode(uint32_t p_ino, uint32_t mode_t);
int vbfs_inode_sync(struct inode_info *inode);
int vbfs_inode_close(struct inode_info *inode);
int vbfs_inode_update_times(struct inode_info *inode, time_update_flags mask);
struct inode_info *pathname_to_inode(const char *pathname);

void fill_stbuf_by_dirent(struct stat *stbuf, struct vbfs_dirent *dirent);
int vbfs_update_times(struct inode_info *inode, time_update_flags mask);
int vbfs_readdir(struct inode_info *inode, off_t filler_pos,
		fuse_fill_dir_t filler, void *filler_buf);
int vbfs_create(struct inode_info *inode, char *subname, uint32_t mode);
int vbfs_truncate(struct inode_info *inode, off_t size);
int vbfs_rmdir(struct inode_info *inode);
int vbfs_unlink(struct inode_info *inode);

#endif
