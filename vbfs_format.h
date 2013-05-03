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
	__u32 inode_count;
	int fd;
};

/* 4k */
struct vbfs_superblock {
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
	__u8 reverse[4];
} __attribute__((packed));

struct inode_bitmap_group {
	__le32 group_no;
	__le32 total_inode;
	__le32 free_inode;
	__le32 current_position;
	__le64 inode_start_offset;
} __attribute__((packed));

struct extend_bitmap_group {
	__le32 group_no;
	__le32 total_extend;
	__le32 free_extend;
	__le32 current_position;
	__le64 extend_start_offset;
} __attribute__((packed));

struct vbfs_inode {
	__le32 i_ino;
	__le32 i_pino; /* what's this */
	__le16 i_mode;
	__le64 i_size;
	__le32 i_atime;
	__le32 i_ctime;
	__le32 i_mtime;

	__le32 i_extends;
} __attribute__((packed));

enum {
	VBFS_FT_UNKOWN,
	VBFS_FT_REG_FILE,
	VBFS_FT_DIR,
	VBFS_FT_MAX
};

struct vbfs_dir_entry {
	__u32 inode;
	__u8 file_type;
	char name[NAME_LEN];
} __attribute__((packed));

struct dir_metadata {
	__le32 dir_count;
	__le32 start_count;
	__le32 next_extend;
} __attribute__((packed));

#endif
