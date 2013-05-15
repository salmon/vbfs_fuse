#ifndef __INODE_H_
#define __INODE_H_

/* open inode */
struct inode_vbfs *vbfs_inode_open(__u32 ino, int *err_no);

/* create inode */
struct inode_vbfs *vbfs_inode_create(__u32 ino, __u32 mode_t, int *err_no);

/* writeback to disk */
int vbfs_inode_sync(struct inode_vbfs *i_vbfs);

/* close inode */
int vbfs_inode_close(struct inode_vbfs *i_vbfs);

/* mark inode dirty */
int vbfs_inode_mark_dirty(struct inode_vbfs *i_vbfs);

/* */

/* update inode time */
int vbfs_inode_update_times(struct inode_vbfs *v_inode, time_update_flags mask);

/* 
 * according father inode and filename to find ino
 * used by vbfs_pathname_to_inode
 * */
int vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode_parent, const char *name, __u32 *ino);

/*
 * according pathname to find inode
 * open/readdir/opendir... while use this function
 * */
struct inode_vbfs *vbfs_pathname_to_inode(const char *pathname, int *err_no);

int inode_get_first_extend_unlocked(struct inode_vbfs *inode_v);

struct inode_vbfs *vbfs_inode_create(__u32 p_ino, __u32 mode_t, int *err_no);

#endif
