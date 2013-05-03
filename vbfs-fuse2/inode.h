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
int vbfs_fuse_update_times(struct inode_vbfs *v_inode, time_update_flags mask);

/* 
 * according father inode and filename to find ino
 * used by vbfs_pathname_to_inode
 * */
__u32 vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode_parent, const char *name, int *err_no);

/*
 * according pathname to find inode
 * open/readdir while use this function
 * */
struct inode_vbfs *vbfs_pathname_to_inode(const char *pathname);

/*
 * static __u32 alloc_extend_bitmap(int *err_no);
 * static __u32 alloc_inode_bitmap(int *err_no);
 * static int alloc_inode(__u32 ino, __u32 p_ino);
 *
 * static int free_extend_bitmap();
 * static int free_inode_bitmap();
 * static 
 * */

#endif
