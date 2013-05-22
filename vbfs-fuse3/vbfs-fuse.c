#define FUSE_USE_VERSION 29

#include "vbfs-fuse.h"
#include "log.h"
#include "mempool.h"
#include "super.h"
#include "inode.h"
#include "dir.h"
#include "direct-io.h"
#include "io-thread.h"

vbfs_fuse_context_t vbfs_ctx;

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
	log_dbg("vbfs_fuse_getattr %s\n", path);

	return 0;
}

static int vbfs_fuse_fgetattr(const char *path, struct stat *stbuf,
				struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_fgetattr\n");

	return 0;
}

static int vbfs_fuse_access(const char *path, int mode)
{
	log_dbg("vbfs_fuse_access\n");

	return 0;
}

static int vbfs_fuse_opendir(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_opendir\n");

	return 0;
}

static int vbfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				off_t offset, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_readdir %s\n", path);

	return 0;
}

static int vbfs_fuse_releasedir(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_releasedir\n");

	return 0;
}

static int vbfs_fuse_mkdir(const char *path, mode_t mode)
{
	log_dbg("vbfs_fuse_mkdir\n");

	return 0;
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

static int vbfs_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_create\n");

	return 0;
}

static int vbfs_fuse_open(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_open\n");

	return 0;
}

static int vbfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_read\n");

	return 0;
}

static int vbfs_fuse_write(const char *path, const char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_write\n");

	return 0;
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
	log_dbg("vbfs_fuse_release\n");

	return 0;
}

static int vbfs_fuse_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_fsync\n");

	return 0;
}

static void *vbfs_fuse_init(struct fuse_conn_info *conn)
{
	log_dbg("vbfs_fuse_init\n");

	return NULL;
}

static void vbfs_fuse_destroy(void *data)
{
	log_dbg("vbfs_fuse_destroy\n");

	rw_thread_close();
	log_close();
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

	ret = init_super(argv[argc - 1]);
	if (ret < 0) {
		fprintf(stderr, "Invalidate filesystem\n");
		exit(1);
	}

	i_argc = argc - 1;
	s_argv = argv;
	s_argv[argc - 1] = NULL;

	ret = rw_thread_init();
	if (ret < 0) {
		fprintf(stderr, "io thread init error\n");
		exit(1);
	}
	ret = rw_thread_open(direct_io, 1);
	if (ret < 0) {
		fprintf(stderr, "io thread create error\n");
		exit(1);
	}

	ret = init_root_inode();
	if (ret < 0) {
		fprintf(stderr, "root inode init error\n");
		exit(1);
	}

	ret = fuse_main(i_argc, s_argv, &vbfs_op, NULL);
	log_err("fuse_main end\n");

	return ret;
}

