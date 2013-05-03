#include "vbfs-fuse.h"
#include "super.h"

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

static int super_load(void)
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

	if (super_load())
		goto err;

	vbfs_ctx.super.s_mount_time = time(NULL);
	vbfs_ctx.super.s_state = 1;
	vbfs_ctx.super.super_vbfs_dirty = 1;

	return 0;

err:
	return -1;
}

int super_sync(void)
{
	int fd;

	fd = vbfs_ctx.fd;
	if(vbfs_ctx.super.super_vbfs_dirty == 0)
		return 0;

	vbfs_superblock_disk->vbfs_super.bad_extend_current = cpu_to_le32(vbfs_ctx.super.bad_extend_current);
	vbfs_superblock_disk->vbfs_super.extend_bitmap_current = cpu_to_le32(vbfs_ctx.super.extend_bitmap_current);
	vbfs_superblock_disk->vbfs_super.inode_bitmap_current = cpu_to_le32(vbfs_ctx.super.inode_bitmap_current);
	vbfs_superblock_disk->vbfs_super.s_mount_time = cpu_to_le32(vbfs_ctx.super.s_mount_time);
	vbfs_superblock_disk->vbfs_super.s_state = cpu_to_le32(vbfs_ctx.super.s_state);

	if (write_to_disk(fd, vbfs_superblock_disk, VBFS_SUPER_OFFSET, VBFS_SUPER_SIZE))
		return -1;
		
	return 0;
}

