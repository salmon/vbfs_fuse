#ifndef __SUPER_H_
#define __SUPER_H_

int init_super(const char *dev_name);
int sync_super(void);
int sync_extend_bitmap(void);
int sync_inode_bitmap(void);

#endif
