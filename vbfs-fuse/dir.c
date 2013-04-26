#include "utils.h"
#include "fuse_vbfs.h"

extern vbfs_fuse_context_t vbfs_ctx;

static int sync_inode()
{
	off64_t offset;
	size_t len;
	int fd;

	fd = vbfs_ctx.vbfs_super->fd;
	len = vbfs_ctx.vbfs_super->s_extend_size;

	if (vbfs_ctx.inode_dirty != 0) {
		offset = vbfs_ctx.inode_offset + vbfs_ctx.inode_cache_extend * len;
		if (write_to_disk(fd, vbfs_ctx.inode_cache, offset, len)) {
			goto err;
		}
		vbfs_ctx.inode_dirty = 0;
	}
	return 0;

err:
	return -1;
}

static int sync_inode_bitmap()
{
	/* Not Implement */

	return 0;
}

static int sync_extend_bitmap()
{
	/* Not Implement */

	return 0;
}

int vbfs_inode_close(struct inode_vbfs *v_inode)
{
	/* Not Implement */
	free(v_inode);
	v_inode = NULL;

	/* sync_all */

	return 0;
}

static int get_first_zero_and_set(char *bitmap, __u32 *pos, __u32 bitmap_size)
{
	__u32 i = 0;
	char *tmp;
	int m = 0;

	tmp = bitmap + *pos;
	/* ffz */
	for (i = 0; i < 0; i ++) {
		if (m == 8) {
			tmp ++;
			m = 0;
		}
		if (! (*tmp & (1 << m))) {
			*tmp = *tmp | (1 << m);
			*pos += (i + 1);
			return 0;
		}
		m ++;
	}


	return -1;
}

static int alloc_inode_bitmap(__u32 *inode_no)
{
	int ret;
	__u32 bitmap_size;

	/* Not implement */
	bitmap_size = vbfs_ctx.vbfs_super->s_extend_size - INODE_BITMAP_META_SIZE;

	ret = get_first_zero_and_set(vbfs_ctx.extend_bitmap_region,
			&vbfs_ctx.inode_info->current_position, bitmap_size);
	if (ret < 0) {
		/* can not find free room */
		/* switch next inode bitmap */
		return -1;
	}
	vbfs_ctx.inode_info->free_inode --;
	log_err(pLog, "inode bitmap alloc %u bit\n", vbfs_ctx.inode_info->current_position);

	*inode_no = vbfs_ctx.inode_info->group_no * vbfs_ctx.inode_info->total_inode
					+ vbfs_ctx.inode_info->current_position;

	vbfs_ctx.inode_bitmap_dirty = 1;

	return 0;
}

static int alloc_extend_bitmap(__u32 *extend_no)
{
	int ret;
	__u32 bitmap_size;

	/* Not implement */
	bitmap_size = vbfs_ctx.vbfs_super->s_extend_size - EXTEND_BITMAP_META_SIZE;

	ret = get_first_zero_and_set(vbfs_ctx.extend_bitmap_region,
			&vbfs_ctx.extend_info->current_position, bitmap_size);
	if (ret < 0) {
		/* can not find free room */
		/* switch next inode bitmap */
		return -1;
	}
	vbfs_ctx.extend_info->free_extend --;
	log_err(pLog, "extend bitmap alloc %u bit\n", vbfs_ctx.extend_info->current_position);

	*extend_no = vbfs_ctx.extend_info->group_no * vbfs_ctx.extend_info->total_extend
					+ vbfs_ctx.extend_info->current_position;

	vbfs_ctx.extend_bitmap_dirty = 1;

	return 0;
}

static int open_inode(__u32 ino, struct inode_vbfs *i_vbfs, int create, int mode)
{
	struct vbfs_inode inode_disk;
	size_t len;
	int fd, need_read = 0;
	off64_t offset;
	char *buf = NULL;
	__u32 skip;

	if (NULL == i_vbfs) {
		goto err;
	}

	memset(i_vbfs, 0, sizeof(struct inode_vbfs));

	fd = vbfs_ctx.vbfs_super->fd;
	len = vbfs_ctx.vbfs_super->s_extend_size;

	skip = ino / vbfs_ctx.inode_count_per_extend;
	offset = vbfs_ctx.inode_offset + (__u64)skip * len;

	if (vbfs_ctx.inode_cache == NULL) {
		if ((buf = Valloc(len)) == NULL) {
			goto err;
		}
		vbfs_ctx.inode_cache = buf;

		vbfs_ctx.inode_cache_extend = skip;
		need_read = 1;
	} else if (vbfs_ctx.inode_cache_extend != skip) {
		vbfs_ctx.inode_cache_extend = skip;
		need_read = 1;
		if (sync_inode()) {
			goto err;
		}
	}

	if (need_read) {
		if (read_from_disk(fd, vbfs_ctx.inode_cache, offset, len)) {
			goto err;
		}
	}

	skip = (ino % vbfs_ctx.inode_count_per_extend) * INODE_SIZE;

	if (create) {
		i_vbfs->i_ino = ino;

		if (alloc_extend_bitmap(&i_vbfs->i_extends)) {
			goto err;
		}

		i_vbfs->i_atime = time(NULL);
		i_vbfs->i_ctime = time(NULL);
		i_vbfs->i_mtime = time(NULL);
		/* HOW TO GET PINO */
		//i_vbfs->pino = XX;

		inode_disk.i_ino = cpu_to_le32(i_vbfs->i_ino);
		inode_disk.i_pino = cpu_to_le32(i_vbfs->i_pino);
		inode_disk.i_mode = cpu_to_le16(i_vbfs->i_mode);
		inode_disk.i_size = cpu_to_le16(i_vbfs->i_size);

		inode_disk.i_atime = cpu_to_le32(i_vbfs->i_atime);
		inode_disk.i_mtime = cpu_to_le32(i_vbfs->i_ctime);
		inode_disk.i_ctime = cpu_to_le32(i_vbfs->i_mtime);

		inode_disk.i_extends = cpu_to_le32(i_vbfs->i_extends);

		memcpy(vbfs_ctx.inode_cache + skip, &inode_disk, sizeof(struct vbfs_inode));

		vbfs_ctx.inode_dirty = 1;

		return 0;
	}

	memcpy(&inode_disk, vbfs_ctx.inode_cache + skip , sizeof(struct vbfs_inode));

	i_vbfs->i_ino = le32_to_cpu(inode_disk.i_ino);
	i_vbfs->i_pino = le32_to_cpu(inode_disk.i_pino);
	i_vbfs->i_mode = le16_to_cpu(inode_disk.i_mode);
	i_vbfs->i_size = le16_to_cpu(inode_disk.i_size);

	i_vbfs->i_atime = le32_to_cpu(inode_disk.i_atime);
	i_vbfs->i_ctime = le32_to_cpu(inode_disk.i_ctime);
	i_vbfs->i_mtime = le32_to_cpu(inode_disk.i_mtime);

	i_vbfs->i_extends = le32_to_cpu(inode_disk.i_extends);

	return 0;

err:
	if (NULL != buf) {
		free(buf);
		buf = NULL;
	}

	return -1;
}

static int create_inode(struct inode_vbfs *i_vbfs, int mode)
{
	__u32 ino;
	int ret;

	ret = alloc_inode_bitmap(&ino);
	if (ret < 0) {
		return ret;
	}

	ret = open_inode(ino, i_vbfs, 1, mode);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int vbfs_readdir(struct inode_vbfs *dir_v_inode, off_t *filler_pos, fuse_fill_dir_t filler, void *filler_buf)
{
	size_t len;
	int fd, i, has_next;
	off64_t offset;
	char *buf = NULL;
	char *pos;
	struct dir_metadata dir_meta_disk;
	struct vbfs_dir_entry vbfs_dir_disk;
	struct dir_info dir_info;
	struct inode_vbfs v_inode;
	struct stat st_buf;


	fd = vbfs_ctx.vbfs_super->fd;
	len = vbfs_ctx.vbfs_super->s_extend_size;
	if ((buf = Valloc(len)) == NULL) {
		goto err;
	}

	offset = vbfs_ctx.extend_offset + (__u64)(dir_v_inode->i_extends) * len;

	if (read_from_disk(fd, buf, offset, len)) {
		goto err;
	}

	memcpy(&dir_meta_disk, buf, sizeof(struct dir_metadata));
	dir_info.dir_count = le32_to_cpu(dir_meta_disk.dir_count);
	dir_info.start_count = le32_to_cpu(dir_meta_disk.start_count);
	dir_info.next_extend = le32_to_cpu(dir_meta_disk.next_extend);
	if (0 != dir_info.next_extend) {
		has_next = 1;
	}

	pos = buf + DIR_META_SIZE;

	for (i = 0; i < dir_info.dir_count; i ++) {
		memset(&st_buf, 0, sizeof(st_buf));
		memcpy(&vbfs_dir_disk, pos, sizeof(struct vbfs_dir_entry));
		pos += sizeof(struct vbfs_dir_entry);

		if (open_inode(le32_to_cpu(vbfs_dir_disk.inode), &v_inode, 0, 0)) {
			log_err(pLog, "vbfs_readdir open_inode err\n");
			goto err;
		}

		st_buf.st_ino = v_inode.i_ino;
		if (v_inode.i_mode == VBFS_FT_DIR) {
			st_buf.st_mode = S_IFDIR | 0777;
		} else if (v_inode.i_mode == VBFS_FT_REG_FILE) {
			st_buf.st_mode = S_IFREG | 0777;
		}
		st_buf.st_atime = v_inode.i_atime;
		st_buf.st_mtime = v_inode.i_mtime;
		st_buf.st_ctime = v_inode.i_ctime;

		filler(filler_buf, vbfs_dir_disk.name, &st_buf, 0);
	}

	free(buf);
	buf = NULL;

	return 0;

err:
	if (NULL != buf) {
		free(buf);
		buf = NULL;
	}

	return -1;
}


int vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode, const char *name, int *err)
{
	size_t len;
	int fd, i, has_next;
	off64_t offset;
	char *buf = NULL;
	char *pos;
	struct dir_metadata dir_meta_disk;
	struct vbfs_dir_entry vbfs_dir_disk;
	struct dir_info dir_info;

	*err = 0;
	fd = vbfs_ctx.vbfs_super->fd;
	len = vbfs_ctx.vbfs_super->s_extend_size;
	if ((buf = Valloc(len)) == NULL) {
		goto err;
	}

	offset = vbfs_ctx.extend_offset + (__u64)(v_inode->i_extends) * len;

	if (read_from_disk(fd, buf, offset, len)) {
		goto err;
	}

	memcpy(&dir_meta_disk, buf, sizeof(struct dir_metadata));
	dir_info.dir_count = le32_to_cpu(dir_meta_disk.dir_count);
	dir_info.start_count = le32_to_cpu(dir_meta_disk.start_count);
	dir_info.next_extend = le32_to_cpu(dir_meta_disk.next_extend);
	if (0 != dir_info.next_extend) {
		has_next = 1;
	}

	pos = buf + DIR_META_SIZE;
	for (i = 0; i < dir_info.dir_count; i ++) {
		memcpy(&vbfs_dir_disk, pos, sizeof(struct vbfs_dir_entry));
		pos += sizeof(struct vbfs_dir_entry);

		if (! strncmp(name, vbfs_dir_disk.name, NAME_LEN - 1)) {
			if (open_inode(le32_to_cpu(vbfs_dir_disk.inode), v_inode, 0, 0)) {
				*err = INTERNAL_ERR;
				goto err;
			}

			free(buf);
			buf = NULL;

			return 0;
		}
	}

	free(buf);
	buf = NULL;

	return 0;

err:

	if (NULL != buf) {
		free(buf);
		buf = NULL;
	}

	return -1;
}

struct inode_vbfs *vbfs_pathname_to_inode(const char *pathname)
{
	char *p, *q;
	char *path = NULL;
	struct inode_vbfs *i_vbfs = NULL;
	char *dentry_buf = NULL;
	int ret, err = INTERNAL_ERR;

	if ((i_vbfs = malloc(sizeof(struct inode_vbfs))) == NULL) {
		log_err(pLog, "Out of memory\n");
		goto err;
	}

	path = strdup(pathname);
	if (!path) {
		log_err(pLog, "Out of memory\n");
		goto err;
	}

	p = path;
	while (p && *p && *p == PATH_SEP)
		p ++;

	/* read root dentry */
	dentry_buf = Valloc(vbfs_ctx.vbfs_super->s_extend_size);
	if (open_inode(ROOT_INO, i_vbfs, 0, 0)) {
		goto err;
	}

	if (! strncmp(path, "/", NAME_LEN - 1)) {
		free(path);
		path = NULL;

		return i_vbfs;
	}

	while (p && *p) {
		/* */
		q = strchr(p, PATH_SEP);
		if (q != NULL) {
			*q = '\0';
		}

		/* processing... */
		ret = vbfs_inode_lookup_by_name(i_vbfs, q, &err);
		if (ret < 0)
			goto err;

		if (err != 0) {
			goto err;
		}

		if (q) {
			*q++ = PATH_SEP;
		}
		p = q;
		while (p && *p && *p == PATH_SEP)
			p ++;
	}

	free(path);

	return i_vbfs;

err:
	if (NULL != path) {
		free(path);
		path = NULL;
	}
	if (NULL != dentry_buf) {
		free(dentry_buf);
		dentry_buf = NULL;
	}
	if (NULL != i_vbfs) {
		free(i_vbfs);
		i_vbfs = NULL;
	}

	return NULL;
}

int vbfs_fuse_update_times(struct inode_vbfs *v_inode, time_update_flags mask)
{
	/* Not Implement */
	return 0;
}

int vbfs_mkdir(struct inode_vbfs *i_vbfs_p, const char *path)
{
	int ret = 0;
	int err;
	struct inode_vbfs *i_vbfs = NULL;

	ret = vbfs_inode_lookup_by_name(i_vbfs_p, path, &err);
	if (ret < 0) {
		if ((i_vbfs = malloc(sizeof(struct inode_vbfs))) == NULL) {
			goto err;
		}
		create_inode(i_vbfs, VBFS_FT_DIR);
	}

	return 0;

err:
	return -1;
}

