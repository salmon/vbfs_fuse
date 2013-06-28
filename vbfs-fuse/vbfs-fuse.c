#define FUSE_USE_VERSION 29

#include "vbfs-fuse.h"
#include "err.h"
#include "ioengine.h"
#include "log.h"

static int vbfs_fuse_getattr(const char *path, struct stat *stbuf);
static int vbfs_fuse_fgetattr(const char *path, struct stat *stbuf,
				struct fuse_file_info *fi);
static int vbfs_fuse_access(const char *path, int mode);

static int vbfs_fuse_opendir(const char *path, struct fuse_file_info *fi);
static int vbfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				off_t offset, struct fuse_file_info *fi);
static int vbfs_fuse_releasedir(const char *path, struct fuse_file_info *fi);
static int vbfs_fuse_mkdir(const char *path, mode_t mode);
static int vbfs_fuse_rmdir(const char *path);

static int vbfs_fuse_rename(const char *from, const char *to);
static int vbfs_fuse_truncate(const char *path, off_t size);
static int vbfs_fuse_ftruncate(const char *path, off_t size, struct fuse_file_info *fi);

static int vbfs_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int vbfs_fuse_open(const char *path, struct fuse_file_info *fi);
static int vbfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi);
static int vbfs_fuse_write(const char *path, const char *buf, size_t size, off_t offset,
				struct fuse_file_info *);
static int vbfs_fuse_statfs(const char *path, struct statvfs *stbuf);
static int vbfs_fuse_flush(const char *path, struct fuse_file_info *fi);
static int vbfs_fuse_release(const char *path, struct fuse_file_info *fi);
static int vbfs_fuse_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);

static void *vbfs_fuse_init(struct fuse_conn_info *conn);
static void vbfs_fuse_destroy(void *data);

static struct fuse_operations vbfs_op = {
	.getattr	= vbfs_fuse_getattr,
	.fgetattr	= vbfs_fuse_fgetattr,
	.access		= vbfs_fuse_access,

	.opendir	= vbfs_fuse_opendir,
	.readdir	= vbfs_fuse_readdir,
	.releasedir	= vbfs_fuse_releasedir,
	.mkdir		= vbfs_fuse_mkdir,
	.rmdir		= vbfs_fuse_rmdir,

	.rename		= vbfs_fuse_rename,
	.truncate	= vbfs_fuse_truncate,
	.ftruncate	= vbfs_fuse_ftruncate,

	.create		= vbfs_fuse_create,
	.open		= vbfs_fuse_open,
	.read		= vbfs_fuse_read,
	//.read_buf	= vbfs_fuse_read_buf,
	.write		= vbfs_fuse_write,
	//.write_buf	= vbfs_fuse_write_buf,

	.statfs		= vbfs_fuse_statfs,
	.flush		= vbfs_fuse_flush,
	.release	= vbfs_fuse_release,
	.fsync		= vbfs_fuse_fsync,

	.init		= vbfs_fuse_init,
	.destroy	= vbfs_fuse_destroy,
};

static int vbfs_fuse_getattr(const char *path, struct stat *stbuf)
{
	struct inode_info *inode;

	log_dbg("vbfs_fuse_getattr %s\n", path);

	inode = pathname_to_inode(path);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	fill_stbuf_by_dirent(stbuf, inode->dirent);

	vbfs_inode_close(inode);

	return 0;
}

static int vbfs_fuse_fgetattr(const char *path, struct stat *stbuf,
				struct fuse_file_info *fi)
{
	int ret = 0;
	struct inode_info *inode;

	log_dbg("vbfs_fuse_fgetattr\n");

	if (fi->fh) {
		inode = (struct inode_info *) fi->fh;
		fill_stbuf_by_dirent(stbuf, inode->dirent);
	}

	return ret;
}

static int vbfs_fuse_access(const char *path, int mode)
{
	log_dbg("vbfs_fuse_access\n");

	return 0;
}

static int vbfs_fuse_opendir(const char *path, struct fuse_file_info *fi)
{
	struct inode_info *inode = NULL;

	log_dbg("vbfs_fuse_opendir\n");

	inode = pathname_to_inode(path);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	fi->fh = (uint64_t) inode;

	return 0;
}

static int vbfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				off_t offset, struct fuse_file_info *fi)
{
	struct inode_info *inode;

	log_dbg("vbfs_fuse_readdir %s\n", path);

	if (! fi->fh) {
		log_err("BUG");
		return -1;
	}

	inode = (struct inode_info *) fi->fh;
	vbfs_readdir(inode, offset, filler, buf);
	vbfs_update_times(inode, UPDATE_ATIME);

	return 0;
}

static int vbfs_fuse_releasedir(const char *path, struct fuse_file_info *fi)
{
	struct inode_info *inode;

	log_dbg("vbfs_fuse_releasedir\n");

	if (! fi->fh) {
		log_err("BUG");
		return -1;
	}

	inode = (struct inode_info *) fi->fh;
	vbfs_inode_close(inode);

	return 0;
}

static int vbfs_create_obj(const char *path, uint32_t mode)
{
	int ret;
	struct inode_info *inode;

	char last_name[NAME_LEN];
	char *name = NULL;
	char *pos = NULL;

	log_dbg("vbfs_fuse_mkdir %s\n", path);

	memset(last_name, 0, sizeof(last_name));
	name = strdup(path);
	if (NULL == name)
		return -ENOMEM;
	pos = name;

	ret = get_lastname(pos, last_name, PATH_SEP);
	if (ret) {
		free(name);
		return -EINVAL;
	}
	if (strlen(last_name) == 0) {
		free(name);
		return -EEXIST;
	}

	inode = pathname_to_inode(pos);
	free(name);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	ret = vbfs_create(inode, last_name, mode);
	if (ret) {
		vbfs_inode_close(inode);
		return ret;
	}

	vbfs_update_times(inode, UPDATE_ATIME | UPDATE_MTIME);
	vbfs_inode_close(inode);

	return 0;
}

static int vbfs_fuse_mkdir(const char *path, mode_t mode)
{
	int ret;

	ret = vbfs_create_obj(path, VBFS_FT_DIR);

	return ret;
}

static int vbfs_fuse_rmdir(const char *path)
{
	log_dbg("vbfs_fuse_rmdir\n");

	return 0;
}

static int vbfs_fuse_rename(const char *from, const char *to)
{
	log_dbg("vbfs_fuse_rename\n");

	return 0;
}

static int vbfs_fuse_truncate(const char *path, off_t size)
{
	log_dbg("vbfs_fuse_truncate\n");

	return 0;
}

static int vbfs_fuse_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_ftruncate\n");

	return 0;
}

static int vbfs_fuse_open(const char *path, struct fuse_file_info *fi)
{
	int ret = 0;
	struct inode_info *inode;

	log_dbg("vbfs_fuse_open %s\n", path);

	inode = pathname_to_inode(path);
	if (IS_ERR(inode))
		ret = PTR_ERR(inode);

	if (-ENOENT == ret) {
		if (fi->flags & O_CREAT) {
			/* create file type inode */
			ret = vbfs_create_obj(path, VBFS_FT_REG_FILE);
			if (ret)
				return ret;

			inode = pathname_to_inode(path);
			if (IS_ERR(inode))
				ret = PTR_ERR(inode);
			else
				ret = 0;

			log_err("pathname to inode %u, %d", inode->dirent->i_ino, ret);
			if (ret)
				return ret;
		} else
			return ret;
	}

	fi->fh = (uint64_t) inode;

	return 0;
}

static int vbfs_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int ret = 0;
	struct inode_info *inode;

	log_dbg("vbfs_fuse_create %s\n", path);

	ret = vbfs_create_obj(path, VBFS_FT_REG_FILE);
	if (ret)
		return ret;

	inode = pathname_to_inode(path);
	if (IS_ERR(inode))
		ret = PTR_ERR(inode);

	log_dbg("pathname to inode %u, %d", inode->dirent->i_ino, ret);
	if (ret)
		return ret;

	fi->fh = (uint64_t) inode;

	return 0;
}

static int vbfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	int ret = 0;
	struct inode_info *inode;

	log_dbg("vbfs_fuse_read\n");

	if (fi->fh) {
		inode = (struct inode_info *) fi->fh;
		ret = vbfs_read_buf(inode, buf, size, offset);
	}

	return ret;
}

static int vbfs_fuse_write(const char *path, const char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	int ret = 0;
	struct inode_info *inode;

	log_dbg("vbfs_fuse_write %s, %u, %d\n", path, size, offset);

	if (fi->fh) {
		inode = (struct inode_info *) fi->fh;
		ret = vbfs_write_buf(inode, buf, size, offset);
	}

	return ret;
}

static int vbfs_fuse_statfs(const char *path, struct statvfs *stbuf)
{
	log_dbg("vbfs_fuse_statfs\n");

	return 0;
}

static int vbfs_fuse_flush(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_flush\n");

	return 0;
}

static int vbfs_fuse_release(const char *path, struct fuse_file_info *fi)
{
	struct inode_info *inode;
	int ret = 0;

	log_dbg("vbfs_fuse_release\n");

	if (fi->fh) {
		inode = (struct inode_info *) fi->fh;

		ret = vbfs_inode_close(inode);
		if (ret)
			return ret;
	}

	return 0;
}

static int vbfs_fuse_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_fsync\n");

	return 0;
}

static void vbfs_fuse_destroy(void *data)
{
	log_dbg("vbfs_fuse_destroy\n");

	queue_destroy(get_meta_queue());
	queue_destroy(get_data_queue());
	ioengine->io_exit();

	log_close();
}

static void *vbfs_fuse_init(struct fuse_conn_info *conn)
{
	int ret;

	log_dbg("vbfs_fuse_init\n");

	ret = meta_queue_create();
	if (ret) {
		log_err("meta queue create error\n");
		exit(1);
	}

	ret = data_queue_create();
	if (ret) {
		log_err("data queue create error\n");
		exit(1);
	}

	ret = ioengine->io_init();
	if (ret) {
		log_err("io thread init error\n");
		exit(1);
	}

	ret = init_root_inode();
	if (ret < 0) {
		log_err("root inode init error\n");
		exit(1);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int i_argc;
	char **s_argv;

	if (argc < 3) {
		fprintf(stderr, "argument error: %s <mountpoint> [options] <device>\n", argv[0]);
		exit(1);
	}

	log_init();
	rdwr_register();

	ret = init_super(argv[argc - 1]);
	if (ret < 0) {
		fprintf(stderr, "Invalidate filesystem\n");
		exit(1);
	}

	i_argc = argc - 1;
	s_argv = argv;
	s_argv[argc - 1] = NULL;

	ret = fuse_main(i_argc, s_argv, &vbfs_op, NULL);
	log_err("fuse_main end\n");

	return ret;
}

