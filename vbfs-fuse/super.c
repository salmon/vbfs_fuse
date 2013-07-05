#include "vbfs-fuse.h"
#include "super.h"
#include "log.h"
#include "err.h"
#include "extend.h"

static vbfs_fuse_context_t vbfs_ctx;
static vbfs_superblock_dk_t *vbfs_superblock_disk;

static int init_vbfs_ctx(int fd)
{
	int i;

	memset(&vbfs_ctx, 0, sizeof(vbfs_ctx));
	vbfs_ctx.fd = fd;

	vbfs_ctx.active_i.inode_cache = mp_malloc(sizeof(struct hlist_head) << INODE_HASH_BITS);
	if (NULL == vbfs_ctx.active_i.inode_cache) {
		fprintf(stderr, "malloc error, %s\n", strerror(errno));
		return -1;
	}
	for (i = 0; i < 1 << INODE_HASH_BITS; i++)
		INIT_HLIST_HEAD(&vbfs_ctx.active_i.inode_cache[i]);

	INIT_LIST_HEAD(&vbfs_ctx.active_i.inode_list);
	pthread_mutex_init(&vbfs_ctx.active_i.lock, NULL);

	return 0;
}

static int load_super(void)
{
	vbfs_ctx.super.s_magic = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_magic);
	if (vbfs_ctx.super.s_magic != VBFS_SUPER_MAGIC) {
		fprintf(stderr, "device is not vbfs filesystem\n");
		return -1;
	}

	vbfs_ctx.super.s_extend_size =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_extend_size);
	vbfs_ctx.super.s_extend_count =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_extend_count);
	vbfs_ctx.super.s_file_idx_len =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_file_idx_len);

	vbfs_ctx.super.bad_count =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_count);
	vbfs_ctx.super.bad_extend_count =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_extend_count);
	vbfs_ctx.super.bad_extend_current =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_extend_current);
	vbfs_ctx.super.bad_extend_offset =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_extend_offset);

	vbfs_ctx.super.bitmap_count =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.bitmap_count);
	vbfs_ctx.super.bitmap_current =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.bitmap_current);
	vbfs_ctx.super.bitmap_offset =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.bitmap_offset);

	vbfs_ctx.super.s_ctime =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_ctime);
	vbfs_ctx.super.s_mount_time =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_mount_time);
	vbfs_ctx.super.s_state =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_state);
	if (vbfs_ctx.super.s_state != CLEAN) {
		log_warning("vbfs not umount cleanly last time\n");
	}

	memcpy(vbfs_superblock_disk->vbfs_super.uuid,
		vbfs_ctx.super.uuid, sizeof(vbfs_ctx.super.uuid));

	return 0;
}

static int init_bad_extend(void)
{
	return 0;
}

int init_super(const char *dev_name)
{
	int fd;
	int ret;

	pthread_mutex_init(&vbfs_ctx.super.lock, NULL);

	if ((vbfs_superblock_disk = Valloc(VBFS_SUPER_SIZE)) == NULL)
		goto err;

	memset(vbfs_superblock_disk, 0, VBFS_SUPER_SIZE);
	fd = open(dev_name, O_RDWR | O_DIRECT | O_LARGEFILE);
	if (fd < 0) {
		fprintf(stderr, "open %s error, %s\n", dev_name, strerror(errno));
		goto err;
	}
	ret = init_vbfs_ctx(fd);
	if (ret)
		goto err;

	if (read_from_disk(fd, vbfs_superblock_disk, VBFS_SUPER_OFFSET, VBFS_SUPER_SIZE))
		goto err;

	if (load_super())
		goto err;

	vbfs_ctx.super.bits_bm_capacity =
			(vbfs_ctx.super.s_extend_size - BITMAP_META_SIZE) * CHAR_BIT;

	vbfs_ctx.super.s_mount_time = time(NULL);
	vbfs_ctx.super.s_state = DIRTY;
	vbfs_ctx.super.super_vbfs_dirty = DIRTY;

	/* bad extend init */
	if (init_bad_extend())
		goto err;

	/* calculate free extend */
	/* */

	return 0;

err:
	return -1;
}

static int sync_super_unlocked(void)
{
	int fd;

	fd = vbfs_ctx.fd;
	if (vbfs_ctx.super.super_vbfs_dirty == CLEAN)
		return 0;

	vbfs_superblock_disk->vbfs_super.bad_extend_current =
		cpu_to_le32(vbfs_ctx.super.bad_extend_current);
	vbfs_superblock_disk->vbfs_super.bitmap_current =
		cpu_to_le32(vbfs_ctx.super.bitmap_current);
	vbfs_superblock_disk->vbfs_super.s_mount_time =
		cpu_to_le32(vbfs_ctx.super.s_mount_time);
	vbfs_superblock_disk->vbfs_super.s_state =
		cpu_to_le32(vbfs_ctx.super.s_state);

	/* bad extend array sync */
	/* */

	if (write_to_disk(fd, vbfs_superblock_disk, VBFS_SUPER_OFFSET, VBFS_SUPER_SIZE))
		return -1;

	vbfs_ctx.super.super_vbfs_dirty = CLEAN;
		
	return 0;
}

int sync_super(void)
{
	int ret;

	pthread_mutex_lock(&vbfs_ctx.super.lock);
	ret = sync_super_unlocked();
	pthread_mutex_unlock(&vbfs_ctx.super.lock);

	return ret;
}

int super_umount_clean(void)
{
	int ret;

	pthread_mutex_lock(&vbfs_ctx.super.lock);
	vbfs_ctx.super.s_state = CLEAN;
	ret = sync_super_unlocked();
	pthread_mutex_unlock(&vbfs_ctx.super.lock);

	return ret;
}

uint32_t get_bitmap_curr(void)
{
	uint32_t bm_offset = 0;

	pthread_mutex_lock(&vbfs_ctx.super.lock);
	bm_offset = vbfs_ctx.super.bitmap_current
			+ vbfs_ctx.super.bitmap_offset;
	pthread_mutex_unlock(&vbfs_ctx.super.lock);

	return bm_offset;
}

uint32_t add_bitmap_curr(void)
{
	uint32_t bm_offset = 0;

	pthread_mutex_lock(&vbfs_ctx.super.lock);

	++ vbfs_ctx.super.bitmap_current;
	vbfs_ctx.super.bitmap_current %= vbfs_ctx.super.bitmap_count;
	bm_offset = vbfs_ctx.super.bitmap_current + vbfs_ctx.super.bitmap_offset;

	vbfs_ctx.super.super_vbfs_dirty = DIRTY;

	pthread_mutex_unlock(&vbfs_ctx.super.lock);

	return bm_offset;
}

int meta_queue_create(void)
{
	int ret = 0, reserved_bufs, hash_bits;

	if (vbfs_ctx.super.bitmap_count < BM_RESERVED_MAX)
		reserved_bufs = vbfs_ctx.super.bitmap_count;
	else
		reserved_bufs = BM_RESERVED_MAX;
	hash_bits = 4;

	vbfs_ctx.meta_queue = queue_create(reserved_bufs, hash_bits, 0);
	if (IS_ERR(vbfs_ctx.meta_queue))
		ret = PTR_ERR(vbfs_ctx.meta_queue);

	return ret;
}

int data_queue_create(void)
{
	int ret = 0, reserved_bufs, hash_bits;
	uint32_t data_offset;

	if (vbfs_ctx.super.s_extend_count < DATA_RESERVED_MAX)
		reserved_bufs = vbfs_ctx.super.s_extend_count;
	else
		reserved_bufs = DATA_RESERVED_MAX;
	hash_bits = 10;

	data_offset = vbfs_ctx.super.bitmap_count + vbfs_ctx.super.bitmap_offset;
	vbfs_ctx.data_queue = queue_create(reserved_bufs, hash_bits, data_offset);
	if (IS_ERR(vbfs_ctx.data_queue)) {
		ret = PTR_ERR(vbfs_ctx.data_queue);
	}

	return ret;
}

inline int get_disk_fd(void)
{
	return vbfs_ctx.fd;
}

inline const size_t get_extend_size(void)
{
	return vbfs_ctx.super.s_extend_size;
}

inline uint32_t get_file_idx_size(void)
{
	return vbfs_ctx.super.s_file_idx_len;
}

inline uint32_t get_file_max_index(void)
{
	return vbfs_ctx.super.s_file_idx_len / 4;
}

inline uint32_t get_bitmap_offset(void)
{
	return vbfs_ctx.super.bitmap_offset;
}

inline struct queue *get_meta_queue(void)
{
	return vbfs_ctx.meta_queue;
}

inline struct queue *get_data_queue(void)
{
	return vbfs_ctx.data_queue;
}

inline struct active_inode *get_active_inode(void)
{
	return &vbfs_ctx.active_i;
}

void init_dir_bm_size(uint32_t dir_bm_size)
{
	vbfs_ctx.super.dir_bm_size = dir_bm_size;
}

void init_dir_capacity(uint32_t dir_capacity)
{
	vbfs_ctx.super.dir_capacity = dir_capacity;
}

inline uint32_t get_dir_bm_size(void)
{
	return vbfs_ctx.super.dir_bm_size;
}

inline uint32_t get_dir_capacity(void)
{
	return vbfs_ctx.super.dir_capacity;
}

inline uint32_t get_bitmap_capacity(void)
{
	return vbfs_ctx.super.bits_bm_capacity;
}
