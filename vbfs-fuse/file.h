#ifndef __FILE_H__
#define __FILE_H__

#include "vbfs-fuse.h"

int sync_file(struct inode_info *inode);
int vbfs_read_buf(struct inode_info *inode, char *buf, size_t size, off_t offset);
int vbfs_write_buf(struct inode_info *inode, const char *buf, size_t size, off_t offset);

#endif
