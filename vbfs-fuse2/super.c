#include "vbfs-fuse.h"
#include "super.h"
#include "log.h"

extern vbfs_fuse_context_t vbfs_ctx;
static vbfs_superblock_dk_t *vbfs_superblock_disk;

static void init_vbfs_ctx(int fd)
{
	memset(&vbfs_ctx, 0, sizeof(vbfs_ctx));

	INIT_LIST_HEAD(&vbfs_ctx.inode_cache_list);
	INIT_LIST_HEAD(&vbfs_ctx.active_inode_list);

	pthread_mutex_init(&vbfs_ctx.lock_super, NULL);
	pthread_mutex_init(&vbfs_ctx.lock_inode_cache, NULL);
	pthread_mutex_init(&vbfs_ctx.lock_active_inode, NULL);

	vbfs_ctx.fd = fd;
}

static int load_super(void)
{
	vbfs_ctx.super.s_magic = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_magic);
	if (vbfs_ctx.super.s_magic != VBFS_SUPER_MAGIC) {
		fprintf(stderr, "device is not vbfs filesystem\n");
		return -1;
	}

	vbfs_ctx.super.s_extend_size = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_extend_size);
	vbfs_ctx.super.s_extend_count = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_extend_count);
	vbfs_ctx.super.s_inode_count = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_inode_count);
	vbfs_ctx.super.s_file_idx_len = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_file_idx_len);

	vbfs_ctx.super.bad_count = le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_count);
	vbfs_ctx.super.bad_extend_count = le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_extend_count);
	vbfs_ctx.super.bad_extend_current = le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_extend_current);
	vbfs_ctx.super.bad_extend_offset = le32_to_cpu(vbfs_superblock_disk->vbfs_super.bad_extend_offset);

	vbfs_ctx.super.extend_bitmap_count = le32_to_cpu(vbfs_superblock_disk->vbfs_super.extend_bitmap_count);
	vbfs_ctx.super.extend_bitmap_current = le32_to_cpu(vbfs_superblock_disk->vbfs_super.extend_bitmap_current);
	vbfs_ctx.super.extend_bitmap_offset = le32_to_cpu(vbfs_superblock_disk->vbfs_super.extend_bitmap_offset);

	vbfs_ctx.super.inode_bitmap_count = le32_to_cpu(vbfs_superblock_disk->vbfs_super.inode_bitmap_count);
	vbfs_ctx.super.inode_bitmap_current = le32_to_cpu(vbfs_superblock_disk->vbfs_super.inode_bitmap_current);
	vbfs_ctx.super.inode_bitmap_offset = le32_to_cpu(vbfs_superblock_disk->vbfs_super.inode_bitmap_offset);

	vbfs_ctx.super.s_ctime = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_ctime);
	vbfs_ctx.super.s_mount_time = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_mount_time);
	vbfs_ctx.super.s_state = le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_state);

	memcpy(vbfs_superblock_disk->vbfs_super.uuid, vbfs_ctx.super.uuid, sizeof(vbfs_ctx.super.uuid));

	return 0;
}

static int init_bad_extend(void)
{
	return 0;
}

static int init_inode_bitmap(void)
{
	__u32 count;
	__u32 i;
	struct inode_bitmap_cache *tmp;

	count = vbfs_ctx.super.inode_bitmap_count;
	vbfs_ctx.inode_bitmap_array =
		Malloc(count * sizeof(struct inode_bitmap_cache));
	if (vbfs_ctx.inode_bitmap_array == NULL) {
		return -1;
	}

	tmp = vbfs_ctx.inode_bitmap_array;
	for (i = 0; i < count; i ++) {
		tmp->extend_no = 0;
		tmp->content = NULL;
		memset(&tmp->inode_bm_info, 0, sizeof(struct inode_bitmap_info));
		tmp->inode_bitmap_region = NULL;
		tmp->cache_status = 0;
		tmp->inode_bitmap_dirty = 0;
		pthread_mutex_init(&tmp->lock_ino_bm_cache, NULL);

		++ tmp;
	}

	return 0;
}

static int init_extend_bitmap(void)
{
	__u32 count;
	__u32 i;
	struct extend_bitmap_cache *tmp;

	count = vbfs_ctx.super.extend_bitmap_count;
	vbfs_ctx.extend_bitmap_array =
		Malloc(count * sizeof(struct extend_bitmap_cache));

	for (i = 0; i < count; i ++) {
		tmp->extend_no = 0;
		tmp->content = NULL;
		memset(&tmp->extend_bm_info, 0, sizeof(struct extend_bitmap_info));
		tmp->extend_bitmap_region = NULL;
		tmp->cache_status = 0;
		tmp->extend_bitmap_dirty = 0;
		pthread_mutex_init(&tmp->lock_ext_bm_cache, NULL);

		++ tmp;
	}

	return 0;
}

int init_super(const char *dev_name)
{
	int fd;

	if ((vbfs_superblock_disk = Valloc(VBFS_SUPER_SIZE)) == NULL)
		goto err;

	memset(vbfs_superblock_disk, 0, VBFS_SUPER_SIZE);
	fd = open(dev_name, O_RDWR | O_DIRECT | O_LARGEFILE);
	if (fd < 0) {
		fprintf(stderr, "open %s error, %s\n", dev_name, strerror(errno));
		goto err;
	}
	init_vbfs_ctx(fd);

	if (read_from_disk(fd, vbfs_superblock_disk, VBFS_SUPER_OFFSET, VBFS_SUPER_SIZE))
		goto err;

	if (load_super())
		goto err;

	vbfs_ctx.super.s_mount_time = time(NULL);
	vbfs_ctx.super.s_state = 1;
	vbfs_ctx.super.super_vbfs_dirty = 1;

	/* bad extend init */
	init_bad_extend();

	/* calculate free extend */
	/* */

	init_inode_bitmap();
	init_extend_bitmap();

	return 0;

err:
	return -1;
}

static int sync_super_locked(void)
{
	int fd;

	fd = vbfs_ctx.fd;
	if (vbfs_ctx.super.super_vbfs_dirty == 0)
		return 0;

	vbfs_superblock_disk->vbfs_super.bad_extend_current = cpu_to_le32(vbfs_ctx.super.bad_extend_current);
	vbfs_superblock_disk->vbfs_super.extend_bitmap_current = cpu_to_le32(vbfs_ctx.super.extend_bitmap_current);
	vbfs_superblock_disk->vbfs_super.inode_bitmap_current = cpu_to_le32(vbfs_ctx.super.inode_bitmap_current);
	vbfs_superblock_disk->vbfs_super.s_mount_time = cpu_to_le32(vbfs_ctx.super.s_mount_time);
	vbfs_superblock_disk->vbfs_super.s_state = cpu_to_le32(vbfs_ctx.super.s_state);

	/* bad extend array sync */
	/* */

	if (write_to_disk(fd, vbfs_superblock_disk, VBFS_SUPER_OFFSET, VBFS_SUPER_SIZE))
		return -1;

	vbfs_ctx.super.super_vbfs_dirty = 0;
		
	return 0;
}

int sync_super(void)
{
	int ret;

	pthread_mutex_lock(&vbfs_ctx.lock_super);
	ret = sync_super_locked();
	pthread_mutex_unlock(&vbfs_ctx.lock_super);

	return ret;
}

static int write_back_ext_bm(struct extend_bitmap_cache *ext_bmc)
{
	if (ext_bmc->cache_status == 0) {
		log_err("BUG !!\n");
		ext_bmc->extend_bitmap_dirty = 0;
		return -1;
	}

	if (write_extend(ext_bmc->extend_no, ext_bmc->content)) {
		return -1;
	}

	ext_bmc->extend_bitmap_dirty = 0;

	return 0;
}

int sync_extend_bitmap(void)
{
	__u32 i, count;
	struct extend_bitmap_cache *tmp;

	count = vbfs_ctx.super.extend_bitmap_count;
	tmp = vbfs_ctx.extend_bitmap_array;

	for (i = 0; i < count; i ++) {
		pthread_mutex_lock(&tmp->lock_ext_bm_cache);
		if (tmp->extend_bitmap_dirty) {
			write_back_ext_bm(tmp);
		}
		pthread_mutex_unlock(&tmp->lock_ext_bm_cache);

		++ tmp;
	}

	return 0;
}

static int write_back_ino_bm(struct inode_bitmap_cache *ino_bmc)
{
	if (ino_bmc->cache_status == 0) {
		log_err("BUG !!\n");
		ino_bmc->inode_bitmap_dirty = 0;
		return -1;
	}

	if (write_extend(ino_bmc->extend_no, ino_bmc->content)) {
		return -1;
	}

	ino_bmc->inode_bitmap_dirty = 0;

	return 0;
}

int sync_inode_bitmap(void)
{
	__u32 i, count;
	struct inode_bitmap_cache *tmp;

	count = vbfs_ctx.super.inode_bitmap_count;
	tmp = vbfs_ctx.inode_bitmap_array;

	for (i = 0; i < count; i ++) {
		pthread_mutex_lock(&tmp->lock_ino_bm_cache);
		if (tmp->inode_bitmap_dirty) {
			write_back_ino_bm(tmp);
		}
		pthread_mutex_unlock(&tmp->lock_ino_bm_cache);

		++ tmp;
	}

	return 0;
}
