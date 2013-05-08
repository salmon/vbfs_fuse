#ifndef __VBFS_FORMAT_H_
#define __VBFS_FORMAT_H_

#include <endian.h>
#include <byteswap.h>
#include "vbfs_fs.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16_to_cpu(x)  ((__u16)(x))
#define le32_to_cpu(x)  ((__u32)(x))
#define le64_to_cpu(x)  ((__u64)(x))
#define cpu_to_le16(x)  ((__u16)(x))
#define cpu_to_le32(x)  ((__u32)(x))
#define cpu_to_le64(x)  ((__u64)(x))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le16_to_cpu(x)  bswap_16(x)
#define le32_to_cpu(x)  bswap_32(x)
#define le64_to_cpu(x)  bswap_64(x)
#define cpu_to_le16(x)  bswap_16(x)
#define cpu_to_le32(x)  bswap_32(x)
#define cpu_to_le64(x)  bswap_64(x)
#endif

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


struct vbfs_paramters {
	__u32 extend_size_kb;
	__u64 total_size;
	char *dev_name;
	int inode_ratio;
	int bad_ratio;
	int file_idx_len;

	__u32 inode_extend_cnt;

	int fd;
};

/* 4k */
struct vbfs_superblock {
/* 
 * except 
 * all is in one extend of unit
 * */
	__u32 s_magic;

	__u32 s_extend_size;
	__u32 s_extend_count;
	__u32 s_inode_count;
	__u32 s_file_idx_len;

	__u32 bad_count;
	__u32 bad_extend_count;
	__u32 bad_extend_current;
	__u32 bad_extend_offset;

	/* extend bitmap use extend nums */
	__u32 extend_bitmap_count; /* depend disk size */
	__u32 extend_bitmap_current;
	__u32 extend_bitmap_offset;

	/* inonde bitmap use extend nums */
	__u32 inode_bitmap_count; /* default 1 */
	__u32 inode_bitmap_offset;
	__u32 inode_bitmap_current;

	__u32 s_ctime; /* the time of mkfs */
	__u32 s_mount_time; /* the time of last mount */
	__u32 s_state; /* clean or unclean */
	__u8 uuid[16];
};

struct inode_bitmap_group {
	__u32 group_no;
	__u32 total_inode;
	__u32 free_inode;
	__u32 current_position;
};

struct extend_bitmap_group {
	__u32 group_no;
	__u32 total_extend;
	__u32 free_extend;
	__u32 current_position;
};

struct vbfs_inode {
	__u32 i_ino;
	__u32 i_pino;
	__u32 i_mode;
	__u64 i_size;
	__u32 i_atime;
	__u32 i_ctime;
	__u32 i_mtime;

	__u32 i_extend;
};

enum {
	VBFS_FT_UNKOWN,
	VBFS_FT_REG_FILE,
	VBFS_FT_DIR,
	VBFS_FT_MAX
};

struct vbfs_dir_entry {
	__u32 inode;
	__u32 file_type;
	char name[NAME_LEN];
};

struct dir_metadata {
	__u32 group_no;
	__u32 total_extends;

	__u32 dir_self_count;
	__u32 dir_total_count;

	__u32 next_extend;
	__u32 dir_capacity;
	__u32 bitmap_size;
};

#endif
