#define FUSE_USE_VERSION 29

#include "utils.h"
#include "fuse_vbfs.h"
#include "dir.h"

vbfs_fuse_context_t vbfs_ctx = {
	.inode_bitmap_region = NULL,
	.inode_bitmap_dirty = 0,

	.extend_bitmap_region = NULL,
	.extend_bitmap_dirty = 0,

	.inode_cache = NULL,
	.inode_dirty = 0,
};

static int vbfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	off_t pos;
	struct inode_vbfs *v_inode;
	log_err(pLog, "vbfs_fuse_readdir path %s\n", path);

	v_inode = vbfs_pathname_to_inode(path);

	if (!v_inode) {
		return -ENOENT;
	}

	if (vbfs_readdir(v_inode, &pos, filler, buf)) {
		return -INTERNAL_ERR;
	}

	vbfs_fuse_update_times(v_inode, UPDATE_ATIME);

	if (vbfs_inode_close(v_inode)) {
		return -INTERNAL_ERR;
	}

	return 0;
}

static int vbfs_fuse_rmdir(const char *path)
{
	log_err(pLog, "vbfs_rmdir %s\n", path);

	return 0;
}

static int vbfs_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	off_t pos;
	struct inode_vbfs *v_inode_parent = NULL;
	struct inode_vbfs *v_inode = NULL;
	char *pathname = NULL, *p, *subdir = NULL;
	int ret = 0;
	int len;

	log_err(pLog, "vbfs_create %s\n", path);

	len = strlen(path);
	if (len > NAME_LEN - 1) {
		ret = ENAMETOOLONG;
		goto err;
	}
	pathname = strdup(path);
	if (! pathname) {
		ret = INTERNAL_ERR;
		goto err;
	}

	while (p[len] == PATH_SEP) {
		if (len < 2 || mode != VBFS_FT_DIR) {
			ret = EEXIST;
			goto err;
		}
		p[len] = '\0';
		len --;
	}

	while (len > 0) {
		if (p[len] == PATH_SEP) {
			p[len] = '\0';
			subdir = p + len + 1;
			break;
		}
		len --;
	}

	if (pathname[0] == '\0') {
		v_inode_parent = vbfs_pathname_to_inode("/");
	} else {
		v_inode_parent = vbfs_pathname_to_inode(pathname);
	}

	if (! v_inode_parent) {
		ret = ENOTDIR;
		goto err;
	}


	if (mode == VBFS_FT_DIR) {
		ret = vbfs_mkdir(v_inode_parent, path);
		if (ret == -1) {
			ret = EEXIST;
			goto err;
		}
		if (vbfs_inode_close(v_inode)) {
			ret = INTERNAL_ERR;
			goto err;
		}
	}

	if (mode == VBFS_FT_REG_FILE) {
		ret = 1;
		goto err;
	}

	if (vbfs_inode_close(v_inode_parent)) {
		ret = INTERNAL_ERR;
		goto err;
	}

	return 0;

err:
	free(pathname);

	return -ret;
}

static int vbfs_fuse_mkdir(const char *path, mode_t mode)
{
	log_err(pLog, "vbfs_mkdir %s\n", path);

	return vbfs_fuse_create(path, VBFS_FT_DIR, NULL);
}

static int vbfs_fuse_open(const char *path, struct fuse_file_info *fi)
{
	log_err(pLog, "vbfs_open %s\n", path);

	return 0;
}

static int vbfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
			struct fuse_file_info *fi)
{
	log_err(pLog, "vbfs_read %s\n", path);

	return 0;
}

static int vbfs_fuse_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	log_err(pLog, "vbfs_write %s\n", path);

	return 0;
}

static int vbfs_fuse_statfs(const char *path, struct statvfs *stbuf)
{
	log_err(pLog, "vbfs_statfs %s\n", path);

	return 0;
}

static int vbfs_fuse_getattr(const char *path, struct stat *st_buf)
{
	int ret = 0;
	struct inode_vbfs *v_inode = NULL;

	log_err(pLog, "vbfs_fuse_getattr\n");

	memset(st_buf, 0, sizeof(struct stat));
	v_inode = vbfs_pathname_to_inode(path);
	if (NULL == v_inode) {
		return -ENOENT;
	}

	st_buf->st_ino = v_inode->i_ino;
	if (v_inode->i_mode == VBFS_FT_DIR) {
		st_buf->st_mode = S_IFDIR | 0777;
	} else if (v_inode->i_mode == VBFS_FT_REG_FILE) {
		st_buf->st_mode = S_IFREG | 0777;
	}
	st_buf->st_atime = v_inode->i_atime;
	st_buf->st_mtime = v_inode->i_mtime;
	st_buf->st_ctime = v_inode->i_ctime;

	vbfs_inode_close(v_inode);

	return ret;
}

static struct fuse_operations vbfs_fuse_op = {
	.getattr = vbfs_fuse_getattr,
	.statfs = vbfs_fuse_statfs,
	.readdir = vbfs_fuse_readdir,
	.mkdir = vbfs_fuse_mkdir,
	.rmdir = vbfs_fuse_rmdir,
	.create = vbfs_fuse_create,
	.open = vbfs_fuse_open,
	.read = vbfs_fuse_read,
	.write = vbfs_fuse_write,
};

static int scan_bad_block()
{
	return 0;
}

static int init_bitmap()
{
	struct inode_bitmap_group inode_bitmap_meta_disk;
	struct extend_bitmap_group extend_bitmap_meta_disk;
	struct inode_bitmap_info *inode_info = NULL;
	struct extend_bitmap_info *extend_info = NULL;
	char *buf;

	/* init extend bitmap */
	if ((extend_info = malloc(sizeof(struct extend_bitmap_info))) == NULL) {
		goto err;
	}
	buf = vbfs_ctx.vbfs_super->inode_bitmap;
	memcpy(&extend_bitmap_meta_disk, buf, sizeof(extend_bitmap_meta_disk));
	vbfs_ctx.extend_bitmap_region = buf + EXTEND_BITMAP_META_SIZE;
	extend_info->group_no = le32_to_cpu(extend_bitmap_meta_disk.group_no);
	extend_info->total_extend = le32_to_cpu(extend_bitmap_meta_disk.total_extend);
	extend_info->free_extend = le32_to_cpu(extend_bitmap_meta_disk.free_extend);
	extend_info->current_position = le32_to_cpu(extend_bitmap_meta_disk.current_position);
	extend_info->extend_start_offset = le64_to_cpu(extend_bitmap_meta_disk.extend_start_offset);

	vbfs_ctx.extend_info = extend_info;

	/* init inode bitmap */
	if ((inode_info = malloc(sizeof(struct inode_bitmap_info))) == NULL) {
		goto err;
	}
	buf = vbfs_ctx.vbfs_super->extend_bitmap;
	memcpy(&inode_bitmap_meta_disk, buf, sizeof(inode_bitmap_meta_disk));
	vbfs_ctx.inode_bitmap_region = buf + INODE_BITMAP_META_SIZE;
	inode_info->group_no = le32_to_cpu(inode_bitmap_meta_disk.group_no);
	inode_info->total_inode = le32_to_cpu(inode_bitmap_meta_disk.total_inode);
	inode_info->free_inode = le32_to_cpu(inode_bitmap_meta_disk.free_inode);
	inode_info->current_position = le32_to_cpu(inode_bitmap_meta_disk.current_position);
	inode_info->inode_start_offset = le64_to_cpu(inode_bitmap_meta_disk.inode_start_offset);

	vbfs_ctx.inode_info = inode_info;

	return 0;

err:
	return -1;
}

static int check_disk_validate(char *device)
{
	char devname[128];
	int fd = -1;
	off64_t offset;
	char *buf = NULL;
	struct vbfs_superblock vbfs_superblock_disk;
	char *inode_bitmap_buf = NULL;
	char *extend_bitmap_buf = NULL;
	struct superblock_vbfs *vbfs_super = NULL;
	size_t len;

	devname[sizeof(devname) - 1] = 0;
	strncpy(devname, device, sizeof(devname) - 1);
	fd = open(devname, O_RDWR | O_DIRECT | O_LARGEFILE);
	if (fd < 0) {
		log_err(pLog, "can't open %s, %s\n", devname, strerror(errno));
		goto err;
	}

	if ((vbfs_super = Valloc(sizeof(struct superblock_vbfs))) == NULL) {
		goto err;
	}
	vbfs_super->fd = fd;

	if ((buf = Valloc(VBFS_SUPER_OFFSET)) == NULL) {
		goto err;
	}
	offset = VBFS_SUPER_OFFSET;
	if (read_from_disk(fd, buf, offset, VBFS_SUPER_OFFSET)) {
		goto err;
	}
	memcpy(&vbfs_superblock_disk, buf, sizeof(vbfs_superblock_disk));

	vbfs_super->s_magic = le32_to_cpu(vbfs_superblock_disk.s_magic);
	if (vbfs_super->s_magic != VBFS_SUPER_MAGIC) {
		log_err(pLog, "magic number not partten\n");
		goto err;
	}

	vbfs_super->s_extend_size = le32_to_cpu(vbfs_superblock_disk.s_extend_size);
	len = vbfs_super->s_extend_size;
	//vbfs_super->s_free_count = ;
	vbfs_super->s_inode_count = le32_to_cpu(vbfs_superblock_disk.s_inode_count);

	vbfs_super->bad_count = le32_to_cpu(vbfs_superblock_disk.bad_count);
	vbfs_super->bad_extend_offset = le32_to_cpu(vbfs_superblock_disk.bad_extend_offset);
	vbfs_super->bad_extend_current = le32_to_cpu(vbfs_superblock_disk.bad_extend_current);

	/* bad block record */
	scan_bad_block();

	vbfs_super->extend_bitmap_count = le32_to_cpu(vbfs_superblock_disk.extend_bitmap_count);
	vbfs_super->extend_bitmap_offset = le32_to_cpu(vbfs_superblock_disk.extend_bitmap_offset);
	vbfs_super->extend_bitmap_current = le32_to_cpu(vbfs_superblock_disk.extend_bitmap_current);

	/* extend bitmap */
	if (NULL == (extend_bitmap_buf = Valloc(vbfs_super->s_extend_size))) {
		goto err;
	}
	offset = (vbfs_super->extend_bitmap_offset + vbfs_super->extend_bitmap_current)
				* vbfs_super->s_extend_size;
	if (read_from_disk(fd, extend_bitmap_buf, offset, len)) {
		goto err;
	}
	vbfs_super->extend_bitmap = extend_bitmap_buf;

	vbfs_super->inode_bitmap_count = le32_to_cpu(vbfs_superblock_disk.inode_bitmap_count);
	vbfs_super->inode_bitmap_offset = le32_to_cpu(vbfs_superblock_disk.inode_bitmap_offset);
	vbfs_super->inode_bitmap_current = le32_to_cpu(vbfs_superblock_disk.inode_bitmap_current);

	/* inode bitmap */
	if (NULL == (inode_bitmap_buf = Valloc(vbfs_super->s_extend_size))) {
		goto err;
	}
	offset = (vbfs_super->inode_bitmap_offset + vbfs_super->inode_bitmap_current)
				* vbfs_super->s_extend_size;
	if (read_from_disk(fd, inode_bitmap_buf, offset, len)) {
		goto err;
	}
	vbfs_super->inode_bitmap = inode_bitmap_buf;

	vbfs_super->s_ctime = le32_to_cpu(vbfs_superblock_disk.s_ctime);
	vbfs_super->s_mount_time = le32_to_cpu(vbfs_superblock_disk.s_mount_time);
	vbfs_super->s_state = le32_to_cpu(vbfs_superblock_disk.s_state);

	vbfs_super->bad_dirty = 0;
	vbfs_super->extend_bitmap_dirty = 0;
	vbfs_super->inode_bitmap_dirty = 0;

	memcpy(vbfs_super->uuid, vbfs_superblock_disk.uuid, sizeof(vbfs_super->uuid));

	vbfs_ctx.vbfs_super = vbfs_super;
	vbfs_ctx.inode_offset = (__u64)(vbfs_super->extend_bitmap_offset
					+ vbfs_super->extend_bitmap_count) * len;
	vbfs_ctx.inode_count_per_extend = len / INODE_SIZE;

	if ((vbfs_super->s_inode_count  * INODE_SIZE) % len) {
		vbfs_ctx.inode_extend_count =  (vbfs_super->s_inode_count  * INODE_SIZE) / len + 1;
	} else {
		vbfs_ctx.inode_extend_count = (vbfs_super->s_inode_count  * INODE_SIZE) / len;
	}
	vbfs_ctx.extend_offset = (__u64)(vbfs_ctx.inode_extend_count + vbfs_super->extend_bitmap_count
					+ vbfs_super->extend_bitmap_offset) * len;

	if (init_bitmap()) {
		goto err;
	}

	free(buf);
	buf = NULL;

	return 0;

err:
	if (fd > 0) {
		close(fd);
		fd = -1;
	}
	if (buf != NULL) {
		free(buf);
		buf = NULL;
	}
	if (vbfs_super != NULL) {
		free(vbfs_super);
		vbfs_super = NULL;
	}
	if (inode_bitmap_buf != NULL) {
		free(inode_bitmap_buf);
		inode_bitmap_buf = NULL;
	}
	if (extend_bitmap_buf != NULL) {
		free(extend_bitmap_buf);
		extend_bitmap_buf = NULL;
	}

	return -1;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int i_argc;
	char **s_argv;

	pLog = fopen("fuse.log", "w+");
	if (argc < 3) {
		log_err(pLog, "argument error: %s <mountpoint> [options] <device>\n", argv[0]);
		exit(1);
	}

	ret = check_disk_validate(argv[argc - 1]);
	if (ret < 0) {
		log_err(pLog, "Invalidate filesystem\n");
		exit(1);
	}

	i_argc = argc - 1;
	s_argv = argv;
	s_argv[argc - 1] = NULL;
	printf("test start\n");
	ret = fuse_main(i_argc, s_argv, &vbfs_fuse_op, NULL);
	printf("test end\n");

	return ret;
}
