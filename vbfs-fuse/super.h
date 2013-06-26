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

	uint32_t dir_bm_size;
	uint32_t dir_capacity;
	uint32_t bits_bm_capacity;
};

inline int get_disk_fd(void);
inline const size_t get_extend_size(void);
inline uint32_t get_file_idx_size(void);
inline uint32_t get_file_max_index(void);
inline struct queue *get_meta_queue(void);
inline struct queue *get_data_queue(void);
inline struct active_inode *get_active_inode(void);
inline uint32_t get_dir_bm_size(void);
inline uint32_t get_dir_capacity(void);
inline uint32_t get_bitmap_capacity(void);

void init_dir_bm_size(uint32_t dir_bm_size);
void init_dir_capacity(uint32_t dir_capacity);
int init_super(const char *dev_name);
int sync_super(void);
uint32_t get_bitmap_curr(void);
uint32_t add_bitmap_curr(void);
int meta_queue_create(void);
int data_queue_create(void);

#endif
