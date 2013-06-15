#ifndef __FILE_H__
#define __FILE_H__

#include "utils.h"

struct extend_data *alloc_edata_by_inode_unlocked(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no);

struct extend_data *alloc_edata_by_inode(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no);

struct extend_data *get_edata_by_inode_unlocked(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no);

struct extend_data *get_edata_by_inode(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no);

int put_edata_by_inode_unlocked(const __u32 extend_no, struct inode_vbfs *inode_v);

int put_edata_by_inode(const __u32 extend_no, struct inode_vbfs *inode_v);

/*
 *
 * */
int file_init_default_fst(struct inode_vbfs *inode_v);
int vbfs_create_file(struct inode_vbfs *v_inode_parent, const char *name);
int sync_file(struct inode_vbfs *inode_v);
int vbfs_read_buf(struct inode_vbfs *inode_v, char *buf, size_t size, off_t offset);
int vbfs_write_buf(struct inode_vbfs *inode_v, const char *buf, size_t size, off_t offset);

#endif
