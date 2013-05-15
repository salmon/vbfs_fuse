#ifndef __SUPER_H_
#define __SUPER_H_

int init_super(const char *dev_name);
int sync_super(void);

int write_back_ext_bm(struct extend_bitmap_cache *ext_bmc);
int sync_extend_bitmap(void);

int write_back_ino_bm(struct inode_bitmap_cache *ino_bmc);
int sync_inode_bitmap(void);

#endif
