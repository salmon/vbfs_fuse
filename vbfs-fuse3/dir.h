#ifndef __DIR_H__
#define __DIR_H__

struct dentry_info {
	__u32 group_no;
	__u32 total_extends;

	__u32 dir_self_count;
	__u32 dir_total_count;

	__u32 next_extend;
	__u32 dir_capacity;
	__u32 bitmap_size; /* 512 bytes as a unit */

	char *dentry_bitmap;
};

struct inode_dirents {
	__u32 ino;

	struct list_head dir_list;

	/* not used by now, may use for cache */
	int ref;
	int dir_cnt;
	int dirent_lock;
	int dirty;
};

/* */
struct dentry_vbfs {
	__u32 inode;
	__u32 file_type;

	char name[NAME_LEN];
	struct list_head dentry_list;
};

int vbfs_mkdir(struct inode_vbfs *v_inode_parent, const char *dirname);

int vbfs_readdir(struct inode_vbfs *inode_v, off_t *filler_pos,
			fuse_fill_dir_t filler, void *filler_buf);

int vbfs_rmdir(struct inode_vbfs *inode_v, const char *dirname);

int vbfs_dir_is_empty(struct inode_vbfs *inode_v);

int create_default_dirname(struct inode_vbfs *inode_v);

/*
 *
 * */

int get_dentry(struct inode_vbfs *inode_v, struct list_head *dir_list);

int put_dentry(struct list_head *dir_list);

void fill_stbuf_by_inode(struct stat *stbuf, struct inode_vbfs *inode_v);

int dir_init_default_fst(struct inode_vbfs *inode_v);

int add_dirent(struct inode_vbfs *inode_v, const __u32 ino, const char *name);

int vbfs_mkdir(struct inode_vbfs *v_inode_parent, const char *name);

#endif
