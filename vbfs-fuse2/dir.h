#ifndef __DIR_H_ 
#define __DIR_H_

/* vbfs_mkdir */
int vbfs_mkdir(struct inode_vbfs *v_inode_parent, const char *dirname);

/* vbfs_readdir */
int vbfs_readdir(struct inode_vbfs *inode_v, off_t *filler_pos, fuse_fill_dir_t filler, void *filler_buf);

/* vbfs_rmdir */
int vbfs_rmdir(struct inode_vbfs *inode_v, const char *dirname);

/* vbfs_dir_is_empty */
int vbfs_dir_is_empty(struct inode_vbfs *inode_v);

/* 
 * create (.) (..) dir
 * */
int create_default_dirname(struct inode_vbfs *inode_v);

/*
 * get list of dentry_vbfs of the inode_v
 * */
int get_dentry(struct inode_vbfs *inode_v, struct list_head *dir_list);

int put_dentry(struct list_head *dir_list);

void fill_stbuf_by_inode(struct stat *stbuf, struct inode_vbfs *inode_v);

void init_default_dir(struct inode_vbfs *inode_v, __u32 p_ino);

#endif
