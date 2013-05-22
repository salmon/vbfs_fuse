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

/* */
struct dentry_vbfs {
	__u32 inode;
	__u32 file_type;

	char name[NAME_LEN];
	struct list_head dentry_list;
};

#endif
