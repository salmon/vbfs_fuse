#define FUSE_USE_VERSION 29

#include "vbfs-fuse.h"
#include "log.h"
#include "mempool.h"
#include "super.h"
#include "inode.h"
#include "dir.h"

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
	int ret = 0;
	struct inode_vbfs *inode_v = NULL;

	log_dbg("vbfs_fuse_getattr %s\n", path);

	memset(stbuf, 0, sizeof(struct stat));
	inode_v = vbfs_pathname_to_inode(path, &ret);
	if (ret) {
		return ret;
	}

	fill_stbuf_by_inode(stbuf, inode_v);

	vbfs_inode_close(inode_v);

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
	int ret = 0;
	struct inode_vbfs *inode_v = 0;
	off_t pos = 0;

	log_dbg("vbfs_fuse_readdir %s\n", path);

	inode_v = vbfs_pathname_to_inode(path, &ret);
	if (ret)
		return ret;

	ret = vbfs_readdir(inode_v, &pos, filler, buf);
	if (ret)
		return ret;

	vbfs_inode_update_times(inode_v, UPDATE_ATIME);

	ret = vbfs_inode_close(inode_v);
	if (ret)
		return ret;

	return 0;
}

static int vbfs_fuse_releasedir(const char *path, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_releasedir\n");

	return 0;
}

static int vbfs_fuse_mkdir(const char *path, mode_t mode)
{
	int ret = 0;
	struct inode_vbfs *inode_v = NULL;
	char last_name[NAME_LEN];
	char *name = NULL;
	char *pos = NULL;

	log_dbg("vbfs_fuse_mkdir\n");

	memset(last_name, 0, sizeof(last_name));
	name = strdup(path);
	if (NULL == name) {
		return -ENOMEM;
	}
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

	inode_v = vbfs_pathname_to_inode(pos, &ret);
	if (ret) {
		free(name);
		return ret;
	}

	ret = vbfs_mkdir(inode_v, last_name);
	if (ret) {
		vbfs_inode_close(inode_v);
		free(name);
		return ret;
	}

	vbfs_inode_update_times(inode_v, UPDATE_ATIME | UPDATE_MTIME);

	ret = vbfs_inode_close(inode_v);
	if (ret) {
		free(name);
		return ret;
	}

	free(name);

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

static struct inode_vbfs *vbfs_create_file_fullpath(const char *path, int *err_no)
{
	int ret = 0;
	struct inode_vbfs *v_inode_parent = NULL, *inode_v = NULL;
	char last_name[NAME_LEN];
	char *name = NULL;
	char *pos = NULL;

	memset(last_name, 0, sizeof(last_name));
	name = strdup(path);
	if (NULL == name) {
		*err_no = -ENOMEM;
		return NULL;
	}
	pos = name;

	ret = get_lastname(pos, last_name, PATH_SEP);
	if (ret) {
		free(name);
		*err_no = -EINVAL;
		return NULL;
	}
	if (strlen(last_name) == 0) {
		free(name);
		*err_no = -EEXIST;
		return NULL;
	}

	v_inode_parent = vbfs_pathname_to_inode(pos, &ret);
	if (ret) {
		*err_no = ret;
		free(name);
		return NULL;
	}

	free(name);

	inode_v = vbfs_create_file(v_inode_parent, last_name);
	vbfs_inode_update_times(v_inode_parent, UPDATE_ATIME | UPDATE_MTIME);
	vbfs_inode_close(v_inode_parent);
	if (ret) {
		*err_no = ret;
		return NULL;
	}

	return inode_v;
}

static int vbfs_fuse_open(const char *path, struct fuse_file_info *fi)
{
	int ret = 0;
	struct inode_vbfs *inode_v = NULL;

	log_dbg("vbfs_fuse_open\n");

	inode_v = vbfs_pathname_to_inode(path, &ret);
	if (-ENOENT == ret) {
		if (fi->flags & O_CREAT) {
			/* create file type inode */
			inode_v = vbfs_create_file_fullpath(path, &ret);
			if (ret)
				return ret;
		} else
			return ret;
	}

	fi->fh = (uint64_t) inode_v;

	return 0;
}

static int vbfs_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	log_dbg("vbfs_fuse_create\n");

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
	struct inode_vbfs *inode_v = NULL;
	int ret = 0;

	log_dbg("vbfs_fuse_release\n");

	if (fi->fh) {
		inode_v = (struct inode_vbfs *) fi->fh;
		ret = vbfs_inode_close(inode_v);
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

static void *vbfs_fuse_init(struct fuse_conn_info *conn)
{
	log_dbg("vbfs_fuse_init\n");

	return NULL;
}

static void vbfs_fuse_destroy(void *data)
{
	log_dbg("vbfs_fuse_destroy\n");

	log_close();
}

static void equeue_init(struct extend_queue *equeue, void *args)
{
	INIT_LIST_HEAD(&equeue->all_ed_list);
	pthread_mutex_init(&equeue->all_ed_lock, NULL);

	INIT_LIST_HEAD(&equeue->cache_list);
	pthread_mutex_init(&equeue->cache_lock, NULL);

	equeue->q_private = args;
}

static void ctx_equeue_init()
{
	equeue_init(&vbfs_ctx.extend_bm_queue, NULL);
	equeue_init(&vbfs_ctx.inode_bm_queue, NULL);
	equeue_init(&vbfs_ctx.inode_queue, NULL);
	equeue_init(&vbfs_ctx.data_queue, NULL);
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

	ctx_equeue_init();

	ret = init_root_inode();
	if (ret < 0) {
		fprintf(stderr, "root inode init error\n");
		exit(1);
	}
	vbfs_init_bitmap();

	ret = fuse_main(i_argc, s_argv, &vbfs_op, NULL);
	log_err("fuse_main end\n");

	return ret;
}

