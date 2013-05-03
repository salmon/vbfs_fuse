#ifndef __DIR_H_ 
#define __DIR_H_

/* vbfs_mkdir */
int vbfs_mkdir(struct inode_vbfs *v_inode, const char *dirname);

/* vbfs_readdir */
int vbfs_readdir(struct inode_vbfs *v_inode, off_t *filler_pos, fuse_fill_dir_t filler, void *filler_buf);

/* vbfs_rmdir */
int vbfs_rmdir(struct inode_vbfs *v_inode, const char *dirname);

/* vbfs_dir_is_empty */
int vbfs_dir_is_empty(struct inode_vbfs *v_inode);

/* 
 * create (.) (..) dir
 * */
int create_default_dirname(struct inode_vbfs *v_inode);

/*
 * get list of dentry_vbfs of the v_inode
 * */
int get_dentry(struct inode_vbfs *v_inode, struct list_head *dentry_list);

#endif
