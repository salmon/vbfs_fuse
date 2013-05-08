#define FUSE_USE_VERSION 29

#include "vbfs-fuse.h"
#include "log.h"
#include "mempool.h"
#include "super.h"

vbfs_fuse_context_t vbfs_ctx;

static int vbfs_getattr(const char *path, struct stat *stbuf);
static int vbfs_fgetattr(const char *path, struct stat *stbuf,
				struct fuse_file_info *fi);
static int vbfs_access(const char *path, int mode);

static int vbfs_opendir(const char *path, struct fuse_file_info *fi);
static int vbfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				off_t offset, struct fuse_file_info *fi);
static int vbfs_releasedir(const char *path, struct fuse_file_info *fi);
static int vbfs_mkdir(const char *path, mode_t mode);
static int vbfs_rmdir(const char *path);

static int vbfs_rename(const char *from, const char *to);
static int vbfs_truncate(const char *path, off_t size);
static int vbfs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi);

static int vbfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int vbfs_open(const char *path, struct fuse_file_info *fi);
static int vbfs_read(const char *path, char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi);
static int vbfs_write(const char *path, const char *buf, size_t size, off_t offset,
				struct fuse_file_info *);
static int vbfs_statfs(const char *path, struct statvfs *stbuf);
static int vbfs_flush(const char *path, struct fuse_file_info *fi);
static int vbfs_release(const char *path, struct fuse_file_info *fi);
static int vbfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);

static void *vbfs_init(struct fuse_conn_info *conn);
static void vbfs_destroy(void *data);

static struct fuse_operations vbfs_op = {
	.getattr	= vbfs_getattr,
	.fgetattr	= vbfs_fgetattr,
	.access		= vbfs_access,

	.opendir	= vbfs_opendir,
	.readdir	= vbfs_readdir,
	.releasedir	= vbfs_releasedir,
	.mkdir		= vbfs_mkdir,
	.rmdir		= vbfs_rmdir,

	.rename		= vbfs_rename,
	.truncate	= vbfs_truncate,
	.ftruncate	= vbfs_ftruncate,

	.create		= vbfs_create,
	.open		= vbfs_open,
	.read		= vbfs_read,
	//.read_buf	= vbfs_read_buf,
	.write		= vbfs_write,
	//.write_buf	= vbfs_write_buf,

	.statfs		= vbfs_statfs,
	.flush		= vbfs_flush,
	.release	= vbfs_release,
	.fsync		= vbfs_fsync,

	.init		= vbfs_init,
	.destroy	= vbfs_destroy,
};

static int vbfs_getattr(const char *path, struct stat *stbuf)
{
	log_dbg("vbfs_getattr\n");

	return 0;
}

static int vbfs_fgetattr(const char *path, struct stat *stbuf,
				struct fuse_file_info *fi)
{
	log_dbg("vbfs_fgetattr\n");

	return 0;
}

static int vbfs_access(const char *path, int mode)
{
	log_dbg("vbfs_access\n");

	return 0;
}

static int vbfs_opendir(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_opendir\n");

	return 0;
}

static int vbfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				off_t offset, struct fuse_file_info *fi)
{
	log_dbg("vbfs_readdir\n");

	return 0;
}

static int vbfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_releasedir\n");

	return 0;
}

static int vbfs_mkdir(const char *path, mode_t mode)
{
	log_dbg("vbfs_mkdir\n");

	return 0;
}

static int vbfs_rmdir(const char *path)
{
	log_dbg("vbfs_rmdir\n");
	return 0;
}

static int vbfs_rename(const char *from, const char *to)
{
	log_dbg("vbfs_rename\n");
	return 0;
}

static int vbfs_truncate(const char *path, off_t size)
{
	log_dbg("vbfs_truncate\n");
	return 0;
}

static int vbfs_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	log_dbg("vbfs_ftruncate\n");
	return 0;
}

static int vbfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	log_dbg("vbfs_create\n");
	return 0;
}

static int vbfs_open(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_open\n");
	return 0;
}

static int vbfs_read(const char *path, char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	log_dbg("vbfs_read\n");
	return 0;
}

static int vbfs_write(const char *path, const char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	log_dbg("vbfs_write\n");
	return 0;
}

static int vbfs_statfs(const char *path, struct statvfs *stbuf)
{
	log_dbg("vbfs_statfs\n");
	return 0;
}

static int vbfs_flush(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_flush\n");
	return 0;
}

static int vbfs_release(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_release\n");
	return 0;
}

static int vbfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fsync\n");
	return 0;
}

static void *vbfs_init(struct fuse_conn_info *conn)
{
	log_dbg("vbfs_init\n");

	return NULL;
}

static void vbfs_destroy(void *data)
{
	log_dbg("vbfs_destroy\n");

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

	ret = fuse_main(i_argc, s_argv, &vbfs_op, NULL);

	return ret;
}

