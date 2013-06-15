#ifndef __VBFS_FS_H_
#define __VBFS_FS_H_

#include <linux/types.h>

#define ROOT_INO 0

#define VBFS_SUPER_MAGIC 0xABCDEF01
#define VBFS_SUPER_OFFSET 4096
#define VBFS_SUPER_SIZE 4096

#define BITMAP_META_SIZE 1024

#define VBFS_DIR_SIZE 512
#define VBFS_DIR_META_SIZE 512

#define NAME_LEN 466

enum {
	VBFS_FT_UNKOWN,
	VBFS_FT_REG_FILE,
	VBFS_FT_DIR,
	VBFS_FT_MAX
};

/* Design */
/* 
 * vbfs layout:
 *     |Superblock|Bad 1|...|Bad k|Bitmap 1|.......|Bitmap j|
 *     |Data 1|.........................|Data n|Super backup|
 * */

/*
 * file type layout in a extend:
 *	|Extend Address Array(256K)|User Data(extendsize - 256K)|
 *
 *	256K can record 8G size
 *	default s_file_idx_len is set to 256K
 * */

/*
 * directory type layout in a extend:
 * 	|dirent header|dir bitmap|sub dirname/filename...|
 *
 *	vbfs_dir_entry:
 * 	|sub dirent 1|.........|sub dirent n|
 * */

/* 4k */
struct vbfs_superblock_disk {
/* 
 * except 
 * all is in one extend of unit
 * */
	__le32 s_magic;

	__le32 s_extend_size;
	__le32 s_extend_count;
	__le32 s_file_idx_len;

	__le32 bad_count;
	__le32 bad_extend_count;
	__le32 bad_extend_current;
	__le32 bad_extend_offset;

	/* extend bitmap use extend nums */
	__le32 bitmap_count; /* depend disk size */
	__le32 bitmap_current;
	__le32 bitmap_offset;

	__le32 s_ctime; /* the time of mkfs */
	__le32 s_mount_time; /* the time of last mount */
	__le32 s_state; /* clean or unclean */
	__u8 uuid[16];
};
#define VBFS_SUPER_ST_SIZE sizeof(struct vbfs_superblock_disk)
typedef struct {
	struct vbfs_superblock_disk vbfs_super;
	char padding[VBFS_SUPER_SIZE - VBFS_SUPER_ST_SIZE];
} vbfs_superblock_dk_t;


struct bitmap_header_disk {
	__le32 group_no;
	__le32 total_cnt;
	__le32 free_cnt;
	__le32 current_position;
};
#define BITMAP_ST_SIZE sizeof(struct bitmap_header_disk)
typedef struct {
	struct bitmap_header_disk bitmap_dk;
	char padding[BITMAP_META_SIZE - BITMAP_ST_SIZE];
} bitmap_header_dk_t;


struct vbfs_dirent_disk {
	__le32 i_ino; /* self extend num position */
	__le32 i_pino;

	__le32 i_mode;
	__le64 i_size;
	__le32 i_atime;
	__le32 i_ctime;
	__le32 i_mtime;

	__le32 padding;

	char name[NAME_LEN];
} __attribute__((packed));


struct vbfs_dir_header_disk {
	__le32 group_no;
	__le32 total_extends;

	__le32 dir_self_count;
	__le32 dir_total_count;

	__le32 next_extend;
	__le32 dir_capacity;
	__le32 bitmap_size;
} __attribute__((packed));
#define VBFS_DIR_META_ST_SIZE sizeof(struct vbfs_dir_header_disk)
typedef struct {
	struct vbfs_dir_header_disk vbfs_dir_header;
	char padding[VBFS_DIR_META_SIZE - VBFS_DIR_META_ST_SIZE];
} vbfs_dir_header_dk_t;

#endif
