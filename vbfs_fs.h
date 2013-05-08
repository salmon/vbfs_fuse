#ifndef __VBFS_FS_H_
#define __VBFS_FS_H_

#include <linux/types.h>

#define VBFS_SUPER_MAGIC 0xABCDEF01
#define VBFS_SUPER_OFFSET 4096
#define VBFS_SUPER_SIZE 4096
#define INODE_SIZE 128
#define EXTEND_BITMAP_META_SIZE 1024
#define INODE_BITMAP_META_SIZE 1024
#define VBFS_DIR_SIZE 512
#define VBFS_DIR_META_SIZE 512

#define NAME_LEN 504

enum {
	VBFS_FT_UNKOWN,
	VBFS_FT_REG_FILE,
	VBFS_FT_DIR,
	VBFS_FT_MAX
};

/* Design */
/* 
 * vbfs layout:
 *     |Superblock|Bad 1|...|Bad k|Inode bitmap 1|...|Inode bitmap j|
 *     |Extend bitmap 1|...|Extend bitmap m|Inode 1|........|Inode o|
 *     |Data 1|.................................|Data n|Super backup|
 * */

/*
 * file type layout in a extend:
 *	|Extend Address Array(256K)|User Data(extendsize - 256K)|
 *	256K can record 8G size
 * */

/*
 * directory type layout in a extend:
 * 	|dirent meta|dirs|
 *	vbfs_dir_entry:
 * 	|self dirent(.)|parent dirent(..)|sub dirent 1|...|sub dirent n|
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
	__le32 s_inode_count;
	__le32 s_file_idx_len;

	__le32 bad_count;
	__le32 bad_extend_count;
	__le32 bad_extend_current;
	__le32 bad_extend_offset;

	/* extend bitmap use extend nums */
	__le32 extend_bitmap_count; /* depend disk size */
	__le32 extend_bitmap_current;
	__le32 extend_bitmap_offset;

	/* inonde bitmap use extend nums */
	__le32 inode_bitmap_count; /* default 1 */
	__le32 inode_bitmap_offset;
	__le32 inode_bitmap_current;

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


struct inode_bitmap_group_disk {
	__le32 group_no;
	__le32 total_inode;
	__le32 free_inode;
	__le32 current_position;
};
#define INODE_BITMAP_ST_SIZE sizeof(struct inode_bitmap_group_disk)

typedef struct {
	struct inode_bitmap_group_disk inode_bm_gp;
	char padding[INODE_BITMAP_META_SIZE - INODE_BITMAP_ST_SIZE];
} inode_bitmap_group_dk_t;


struct extend_bitmap_group_disk {
	__le32 group_no;
	__le32 total_extend;
	__le32 free_extend;
	__le32 current_position;
};
#define EXTEND_BITMAP_ST_SIZE sizeof(struct extend_bitmap_group_disk)

typedef struct {
	struct extend_bitmap_group_disk extend_bm_gp;
	char padding[EXTEND_BITMAP_META_SIZE - EXTEND_BITMAP_ST_SIZE];
} extend_bitmap_group_dk_t;


struct vbfs_inode_disk {
	__le32 i_ino;
	__le32 i_pino;
	__le32 i_mode;
	__le64 i_size;
	__le32 i_atime;
	__le32 i_ctime;
	__le32 i_mtime;

	__le32 i_extend;
};
#define VBFS_INODE_ST_SIZE sizeof(struct vbfs_inode_disk)

typedef struct {
	struct vbfs_inode_disk vbfs_inode;
	char padding[INODE_SIZE - VBFS_INODE_ST_SIZE];
} vbfs_inode_dk_t;


struct vbfs_dirent_disk {
	__le32 inode;
	__le32 file_type;
	char name[NAME_LEN];
};


struct vbfs_dir_meta_disk {
	__le32 group_no;
	__le32 total_extends;

	__le32 dir_self_count;
	__le32 dir_total_count;

	__le32 next_extend;
	__le32 dir_capacity;
	__le32 bitmap_size;
} __attribute__((packed));
#define VBFS_DIR_META_ST_SIZE sizeof(struct vbfs_dir_meta_disk)

typedef struct {
	struct vbfs_dir_meta_disk vbfs_dir_meta;
	char dir_bitmap[VBFS_DIR_META_SIZE - VBFS_DIR_META_ST_SIZE];
} vbfs_dir_meta_dk_t;

#endif
