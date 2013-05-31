#include "vbfs-fuse.h"
#include "super.h"
#include "log.h"
#include "extend.h"
#include "mempool.h"

extern vbfs_fuse_context_t vbfs_ctx;
static vbfs_superblock_dk_t *vbfs_superblock_disk;

static void init_vbfs_ctx(int fd)
{
	memset(&vbfs_ctx, 0, sizeof(vbfs_ctx));

	vbfs_ctx.fd = fd;

	pthread_mutex_init(&vbfs_ctx.lock_super, NULL);

	INIT_LIST_HEAD(&vbfs_ctx.active_inode_list);
	pthread_mutex_init(&vbfs_ctx.active_inode_lock, NULL);

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
	vbfs_ctx.super.s_inode_count =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_inode_count);
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

	vbfs_ctx.super.extend_bitmap_count =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.extend_bitmap_count);
	vbfs_ctx.super.extend_bitmap_current =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.extend_bitmap_current);
	vbfs_ctx.super.extend_bitmap_offset =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.extend_bitmap_offset);

	vbfs_ctx.super.inode_bitmap_count =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.inode_bitmap_count);
	vbfs_ctx.super.inode_bitmap_current =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.inode_bitmap_current);
	vbfs_ctx.super.inode_bitmap_offset =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.inode_bitmap_offset);

	vbfs_ctx.super.s_ctime =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_ctime);
	vbfs_ctx.super.s_mount_time =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_mount_time);
	vbfs_ctx.super.s_state =
		le32_to_cpu(vbfs_superblock_disk->vbfs_super.s_state);
	if (vbfs_ctx.super.s_state != MOUNT_CLEAN) {
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
	vbfs_ctx.super.s_state = MOUNT_DIRTY;
	vbfs_ctx.super.super_vbfs_dirty = SUPER_DIRTY;

	vbfs_ctx.super.inode_offset = vbfs_ctx.super.extend_bitmap_offset
					+ vbfs_ctx.super.extend_bitmap_count;
	vbfs_ctx.super.inode_extends = ((__u64)vbfs_ctx.super.s_inode_count * INODE_SIZE)
					/ vbfs_ctx.super.s_extend_size;

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
	if (vbfs_ctx.super.super_vbfs_dirty == SUPER_CLEAN)
		return 0;

	vbfs_superblock_disk->vbfs_super.bad_extend_current =
		cpu_to_le32(vbfs_ctx.super.bad_extend_current);
	vbfs_superblock_disk->vbfs_super.extend_bitmap_current =
		cpu_to_le32(vbfs_ctx.super.extend_bitmap_current);
	vbfs_superblock_disk->vbfs_super.inode_bitmap_current =
		cpu_to_le32(vbfs_ctx.super.inode_bitmap_current);
	vbfs_superblock_disk->vbfs_super.s_mount_time =
		cpu_to_le32(vbfs_ctx.super.s_mount_time);
	vbfs_superblock_disk->vbfs_super.s_state =
		cpu_to_le32(vbfs_ctx.super.s_state);

	/* bad extend array sync */
	/* */

	if (write_to_disk(fd, vbfs_superblock_disk, VBFS_SUPER_OFFSET, VBFS_SUPER_SIZE))
		return -1;

	vbfs_ctx.super.super_vbfs_dirty = SUPER_CLEAN;
		
	return 0;
}

int sync_super(void)
{
	int ret;

	pthread_mutex_lock(&vbfs_ctx.lock_super);
	ret = sync_super_unlocked();
	pthread_mutex_unlock(&vbfs_ctx.lock_super);

	return ret;
}

const size_t get_extend_size()
{
	return vbfs_ctx.super.s_extend_size;
}

__u32 get_extend_bm_curr()
{
	__u32 bm_offset = 0;

	pthread_mutex_lock(&vbfs_ctx.lock_super);
	bm_offset = vbfs_ctx.super.extend_bitmap_current
			+ vbfs_ctx.super.extend_bitmap_offset;
	pthread_mutex_unlock(&vbfs_ctx.lock_super);

	return bm_offset;
}

__u32 get_inode_bm_curr()
{
	__u32 bm_offset = 0;

	pthread_mutex_lock(&vbfs_ctx.lock_super);
	bm_offset = vbfs_ctx.super.inode_bitmap_current
			+ vbfs_ctx.super.inode_bitmap_offset;
	pthread_mutex_unlock(&vbfs_ctx.lock_super);

	return bm_offset;
}

__u32 add_extend_bm_curr()
{
	__u32 bm_offset = 0;

	pthread_mutex_lock(&vbfs_ctx.lock_super);

	++ vbfs_ctx.super.extend_bitmap_current;
	vbfs_ctx.super.extend_bitmap_current %=
			vbfs_ctx.super.extend_bitmap_count;
	bm_offset = vbfs_ctx.super.extend_bitmap_current
			+ vbfs_ctx.super.extend_bitmap_offset;

	pthread_mutex_unlock(&vbfs_ctx.lock_super);

	return bm_offset;
}

__u32 add_inode_bm_curr()
{
	__u32 bm_offset = 0;

	pthread_mutex_lock(&vbfs_ctx.lock_super);

	++ vbfs_ctx.super.inode_bitmap_current;
	vbfs_ctx.super.inode_bitmap_current %=
			vbfs_ctx.super.inode_bitmap_count;
	bm_offset = vbfs_ctx.super.inode_bitmap_current
			+ vbfs_ctx.super.inode_bitmap_offset;

	pthread_mutex_unlock(&vbfs_ctx.lock_super);

	return bm_offset;
}

__u32 get_file_idx_len()
{
	return &vbfs_ctx.super.s_file_idx_len * 1024;
}

__u32 get_file_max_index()
{
	return &vbfs_ctx.super.s_file_idx_len * 256;
}
