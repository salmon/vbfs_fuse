#ifndef __INODE_H__
#define __INODE_H__

#include "utils.h"

typedef enum {
	UPDATE_ATIME = 1 << 0,
	UPDATE_MTIME = 1 << 1,
	UPDATE_CTIME = 1 << 2,
} time_update_flags;

struct vbfs_dirent_header {
	uint32_t group_no;
	uint32_t total_extends;

	uint32_t dir_self_count;
	uint32_t dir_total_count;
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

	char *name;
};

struct extend {
	uint32_t eno;
	struct list_head list;
};

struct inode_info {
	struct vbfs_dirent *dirent;
	uint32_t position;

	int inode_dirty;
	unsigned int flags;
	int ref;
	struct list_head active_list;
	pthread_mutex_t inode_lock;

	struct list_head extend_list;
};

int init_root_inode(void);
struct inode_vbfs *get_root_inode(void);
struct inode_vbfs *vbfs_inode_open(uint32_t ino);
struct inode_vbfs *alloc_inode(uint32_t p_ino, uint32_t mode_t);
int vbfs_inode_sync(struct inode_info *inode);
int vbfs_inode_close(struct inode_info *inode);
int vbfs_inode_update_times(struct inode_info *inode, time_update_flags mask);
int vbfs_inode_lookup_by_name(struct inode_info *v_inode_parent, const char *name);
struct inode_info *vbfs_pathname_to_inode(const char *pathname);

#endif
