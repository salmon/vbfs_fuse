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

struct vbfs_paramters {
	__u32 extend_size_kb;
	__u64 total_size;
	char *dev_name;
	int bad_ratio;
	int file_idx_len;

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
	__u32 s_file_idx_len;

	__u32 bad_count;
	__u32 bad_extend_count;
	__u32 bad_extend_current;
	__u32 bad_extend_offset;

	/* bitmap use extend nums */
	__u32 bitmap_count; /* default 1 */
	__u32 bitmap_offset;
	__u32 bitmap_current;

	__u32 s_ctime; /* the time of mkfs */
	__u32 s_mount_time; /* the time of last mount */
	__u32 s_state; /* clean or unclean */
	__u8 uuid[16];
};

struct bitmap_header {
	__u32 group_no;
	__u32 total_cnt;
	__u32 free_cnt;
	__u32 current_position;
};

struct vbfs_dirent {
	__u32 i_ino;
	__u32 i_pino;
	__u32 i_mode;
	__u64 i_size;
	__u32 i_atime;
	__u32 i_ctime;
	__u32 i_mtime;

	__u32 padding;

	char name[NAME_LEN];
};

struct vbfs_dirent_header {
	__u32 group_no;
	__u32 total_extends;

	__u32 dir_self_count;
	__u32 dir_total_count;

	__u32 next_extend;
	__u32 dir_capacity;
	__u32 bitmap_size; /* 512 bytes is a unit */
};

#endif
