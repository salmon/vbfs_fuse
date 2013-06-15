#ifndef __SUPER_H__
#define __SUPER_H__

#include "utils.h"

struct superblock_vbfs {
	uint32_t s_magic;
	uint32_t s_extend_size;
	uint32_t s_extend_count;
	uint32_t s_file_idx_len;

	uint32_t bad_count;
	uint32_t bad_extend_count;
	uint32_t bad_extend_offset;
	uint32_t bad_extend_current;

	uint32_t bitmap_count;
	uint32_t bitmap_offset;
	uint32_t bitmap_current;

	uint32_t s_ctime;
	uint32_t s_mount_time;
	uint32_t s_state;

	char uuid[16];

	int super_vbfs_dirty;
	uint32_t s_free_count;
	pthread_mutex_t lock;
};

int init_super(const char *dev_name);
int sync_super(void);
const size_t get_extend_size(void);
uint32_t get_bitmap_curr(void);
uint32_t add_bitmap_curr(void);
uint32_t get_file_idx_size(void);
uint32_t get_file_max_index(void);

#endif
