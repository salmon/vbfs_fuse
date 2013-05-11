#include "utils.h"
#include "vbfs-fuse.h"
#include "mempool.h"
#include "inode.h"

int open_extend_data(struct inode_vbfs *inode_v, __u32 extend_no);

int close_extend_data(struct inode_vbfs *inode_v, __u32 extend_no);

int release_extend_data(struct inode_vfbs *inode_v, __u32 extend_no);

int sync_extend_data(struct inode_vbfs *inode_v);

