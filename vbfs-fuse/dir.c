#include "log.h"
#include "utils.h"
#include "mempool.h"
#include "vbfs-fuse.h"

extern vbfs_fuse_context_t vbfs_ctx;

static void load_inode_info(struct inode_vbfs *inode, vbfs_inode_dk_t *inode_disk)
{
	inode->i_ino = le32_to_cpu(inode_disk->vbfs_inode.i_ino);
	inode->i_pino = le32_to_cpu(inode_disk->vbfs_inode.i_pino);
	inode->i_mode = le32_to_cpu(inode_disk->vbfs_inode.i_mode);
	inode->i_size = le64_to_cpu(inode_disk->vbfs_inode.i_size);

	inode->i_ctime = le32_to_cpu(inode_disk->vbfs_inode.i_ctime);
	inode->i_mtime = le32_to_cpu(inode_disk->vbfs_inode.i_mtime);
	inode->i_atime = le32_to_cpu(inode_disk->vbfs_inode.i_atime);

	inode->i_extend = le32_to_cpu(inode_disk->vbfs_inode.i_extend);
}

static void save_inode_info(struct inode_vbfs *inode, vbfs_inode_dk_t *inode_disk)
{
	inode_disk->vbfs_inode.i_ino = cpu_to_le32(inode->i_ino);
	inode_disk->vbfs_inode.i_pino = cpu_to_le32(inode->i_pino);
	inode_disk->vbfs_inode.i_mode = cpu_to_le32(inode->i_mode);
	inode_disk->vbfs_inode.i_size = cpu_to_le64(inode->i_size);

	inode_disk->vbfs_inode.i_ctime = cpu_to_le32(inode->i_ctime);
	inode_disk->vbfs_inode.i_mtime = cpu_to_le32(inode->i_mtime);
	inode_disk->vbfs_inode.i_atime = cpu_to_le32(inode->i_atime);

	inode_disk->vbfs_inode.i_extend = cpu_to_le32(inode->i_extend);
}

/*
 * @ ino is inode number
 * @ extend_no is extend as a unit
 * @ inode_offset is bytes as a unit
 *
 * input ino and then output the extend_no and inode_offset
 * where is the inode_vbfs's storage location
 *
 * Return 0 if success, -1 if ino is invaid
 * */
static int get_inode_position(const __u32 ino, __u32 *extend_no, __u32 *inode_offset)
{
	__u32 extend_offset = 0;
	__u32 inode_cnt = 0;

	if (vbfs_ctx.super.s_inode_count < ino)
		return -1;

	inode_cnt = get_extend_size() / INODE_SIZE;

	extend_offset = ino / inode_cnt;
	*inode_offset = (ino % inode_cnt) * INODE_SIZE;

	*extend_no = vbfs_ctx.super.inode_offset + extend_offset;

	return 0;
}

/*
 * may not need active_inode_lock ?
 *
 * Return root inode
 * */
struct inode_vbfs *get_root_inode()
{
	struct inode_vbfs *root_inode = NULL;

	root_inode = list_first_entry(&vbfs_ctx.active_inode_list,
				struct inode_vbfs, active_list);

	return root_inode;
}

int init_root_inode()
{
	int ret = 0;
	struct inode_vbfs *inode_v = NULL;

	list_add_tail(&inode_v->active_list, &vbfs_ctx.active_inode_list);

	return 0;
}

static struct inode_vbfs *vbfs_inode_open_unlocked(const __u32 ino, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;

	inode_v = get_active_inode(ino);
	if (NULL != inode_v) {
		pthread_mutex_lock(&inode_v->inode_lock);
		inode_v->ref ++;
		pthread_mutex_unlock(&inode_v->inode_lock);
		return inode_v;
	}

	inode_v = open_inode(ino, err_no);
	if (*err_no) {
		return NULL;
	}

	list_add_tail(&inode_v->active_list, &vbfs_ctx.active_inode_list);

	return inode_v;
}

static void fill_default_inode(struct inode_vbfs *inode_v,
			__u32 p_ino, __u32 mode_t, __u32 ino)
{
	inode_v->i_ino = ino;
	inode_v->i_pino = p_ino;
	inode_v->i_mode = mode_t;

	if (mode_t == VBFS_FT_DIR) {
		inode_v->i_size = get_extend_size();
	} else {
		inode_v->i_size = 0;
	}

	inode_v->i_ctime = time(NULL);
	inode_v->i_atime = time(NULL);
	inode_v->i_mtime = time(NULL);
}

static struct inode_vbfs *alloc_inode_unlocked(__u32 p_ino, __u32 mode_t, int *err_no)
{
	struct extend_data *edata = NULL;
	struct inode_vbfs *inode_v = NULL;
	__u32 extend_no = 0, inode_offset = 0;
	int ret = 0;
	char *pos = NULL;
	__u32 ino = 0;

	ret = alloc_inode_bitmap(&ino);
	if (ret) {
		*err_no = ret;
		return NULL;
	}

	if (get_inode_position(ino, &extend_no, &inode_offset)) {
		*err_no = -INTERNAL_ERR;
		goto err;
	}

	edata = open_edata(extend_no, &vbfs_ctx.inode_queue, err_no);
	if (*err_no)
		goto err;

	inode_v = mp_malloc(sizeof(struct inode_vbfs));
	if (NULL == inode_v) {
		*err_no = -ENOMEM;
		goto err;
	}
	fill_default_inode(inode_v, p_ino, mode_t, ino);
	ret = alloc_extend_bitmap(&inode_v->i_extend);
	if (ret) {
		*err_no = ret;
		goto destroy_edata;
	}

	read_edata(edata);
	if (BUFFER_NOT_READY != edata->status) {
		pthread_mutex_lock(&edata->ed_lock);

		pos = edata->buf + inode_offset;
		save_inode_info(inode_v, (vbfs_inode_dk_t *) pos);
		edata->status = BUFFER_DIRTY;

		pthread_mutex_unlock(&edata->ed_lock);
	} else {
		*err_no = -EIO;
		goto destroy_edata;
	}

	close_edata(edata);

	inode_v->inode_dirty = INODE_CLEAN;
	inode_v->ref = 1;
	INIT_LIST_HEAD(&inode_v->active_list);
	pthread_mutex_init(&inode_v->inode_lock, NULL);

	init_inode_dirent(&inode_v->dirent);
	INIT_LIST_HEAD(&inode_v->data_buf_list);

	init_default_fst_extend(inode_v);

	list_add_tail(&inode_v->active_list, &vbfs_ctx.active_inode_list);

	return inode_v;

destroy_edata:
	close_edata(edata);
	mp_free(inode_v);

err:
	return NULL;
}

struct inode_vbfs *alloc_inode(__u32 p_ino, __u32 mode_t, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;

	pthread_mutex_lock(&vbfs_ctx.active_inode_lock);
	inode_v = alloc_inode_unlocked(p_ino, mode_t, err_no);
	pthread_mutex_unlock(&vbfs_ctx.active_inode_lock);

	return inode_v;
}

static int vbfs_inode_free(struct inode_vbfs *inode_v)
{
	struct extend_data *edata = NULL;
	struct extend_data *tmp = NULL;
	int ret = 0;

	pthread_mutex_lock(&inode_v->inode_lock);
	list_for_each_entry_safe(edata, tmp, &inode_v->data_buf_list, data_list) {
		if (edata->inode_ref != 0) {
			log_err("BUG\n");
		}
		ret = close_edata(edata);
		list_del(&edata->data_list);
	}

	if (INODE_DIRTY == inode_v->inode_dirty)
		ret = writeback_inode(inode_v);

	pthread_mutex_unlock(&inode_v->inode_lock);

	pthread_mutex_destroy(&inode_v->inode_lock);
	mp_free(inode_v);

	return ret;
}

int vbfs_inode_close(struct inode_vbfs *inode_v)
{
	struct inode_vbfs *i_vbfs = NULL;
	struct inode_vbfs *tmp = NULL;

	if (ROOT_INO == inode_v->i_ino) {
		//vbfs_inode_sync(inode_v);
		return 0;
	}

	pthread_mutex_lock(&vbfs_ctx.active_inode_lock);

	list_for_each_entry_safe(i_vbfs, tmp, &vbfs_ctx.active_inode_list, active_list) {
		if (i_vbfs->i_ino == inode_v->i_ino) {
			pthread_mutex_lock(&inode_v->inode_lock);
			if (--inode_v->ref == 0) {
				list_del(&inode_v->active_list);
				if (VBFS_FT_DIR == inode_v->i_mode) {
					put_dentry_unlocked(inode_v);
				}
				pthread_mutex_unlock(&inode_v->inode_lock);
				vbfs_inode_free(inode_v);
			} else {
				pthread_mutex_unlock(&inode_v->inode_lock);
			}
		}
	}

	pthread_mutex_unlock(&vbfs_ctx.active_inode_lock);

	return 0;
}

int vbfs_inode_sync(struct inode_vbfs *inode_v)
{
	if (VBFS_FT_DIR == inode_v->i_mode) {
		sync_dentry(inode_v);
	} else if (VBFS_FT_REG_FILE == inode_v->i_mode) {
		sync_file(inode_v);
	}

	return 0;
}

static void load_dirent_header(vbfs_dir_header_dk_t *dh_dk, struct vbfs_dir_header *dh)
{
	dh->group_no = le32_to_cpu(dh_dk->vbfs_dir_header.group_no);
	dh->total_extends = le32_to_cpu(dh_dk->vbfs_dir_header.total_extends);
	dh->dir_self_count = le32_to_cpu(dh_dk->vbfs_dir_header.dir_self_count);
	dh->dir_total_count = le32_to_cpu(dh_dk->vbfs_dir_header.dir_total_count);
	dh->next_extend = le32_to_cpu(dh_dk->vbfs_dir_header.next_extend);
	dh->dir_capacity = le32_to_cpu(dh_dk->vbfs_dir_header.dir_capacity);
	dh->bitmap_size = le32_to_cpu(dh_dk->vbfs_dir_header.bitmap_size);
}

static void save_dirent_header(vbfs_dir_header_dk_t *dh_dk, struct vbfs_dir_header *dh)
{
	memset(dh_dk, 0, sizeof(*dh_dk));

	dh_dk->vbfs_dir_header.group_no = cpu_to_le32(dh->group_no);
	dh_dk->vbfs_dir_header.total_extends = cpu_to_le32(dh->total_extends);
	dh_dk->vbfs_dir_header.dir_self_count = cpu_to_le32(dh->dir_self_count);
	dh_dk->vbfs_dir_header.dir_total_count = cpu_to_le32(dh->dir_total_count);
	dh_dk->vbfs_dir_header.next_extend = cpu_to_le32(dh->next_extend);
	dh_dk->vbfs_dir_header.dir_capacity = cpu_to_le32(dh->dir_capacity);
	dh_dk->vbfs_dir_header.bitmap_size = cpu_to_le32(dh->bitmap_size);
}

static void save_dirent(struct vbfs_dirent_disk *dir_dk, struct vbfs_dirent *dir)
{
	dir->i_ino = le32_to_cpu(dir_dk->i_ino);
	dir->i_pino = le32_to_cpu(dir_dk->i_pino);
	dir->i_mode = le32_to_cpu(dir_dk->i_mode);
	dir->i_size = le64_to_cpu(dir_dk->i_size);
	dir->i_atime = le32_to_cpu(dir_dk->i_atime);
	dir->i_ctime = le32_to_cpu(dir_dk->i_ctime);
	dir->i_mtime = le32_to_cpu(dir_dk->i_mtime);

	dir->name = mp_malloc(strlen(dir_dk->name) + 1);
	strncpy(dir->name, dir_dk->name, NAME_LEN);
}

static void load_dirent(struct vbfs_dirent_disk *dir_dk, struct vbfs_dirent *dir)
{
	memset(dir_dk, 0, sizeof(*dir_dk));

	dir_dk->i_ino = cpu_to_le32(dir->i_ino);
	dir_dk->i_pino = cpu_to_le32(dir->i_pino);
	dir_dk->i_mode = cpu_to_le32(dir->i_mode);
	dir_dk->i_size = cpu_to_le32(dir->i_size);
	dir_dk->i_atime = cpu_to_le32(dir->i_atime);
	dir_dk->i_ctime = cpu_to_le32(dir->i_ctime);
	dir_dk->i_mtime = cpu_to_le32(dir->i_mtime);

	strncpy(dir_dk->name, dir->name, NAME_LEN);
}

static void find_inode(struct extend_buf *b, const char *subname)
{
	char *data, *pos;
	struct vbfs_dirent dir;
	struct vbfs_dir_header dir_header;
	struct bitmap bm;

	bm.map_len = dir_header.bitmap_size;
	bm.max_bits = dir_header.dir_capacity;
	bm.bitmap = b->data + VBFS_DIR_META_SIZE;

	return;
}


struct inode_info *inode_lookup_by_name(struct inode_info *inode_parent, const char *subname)
{
	int ret = 0;
	char *data;
	struct extend_buf *ebuf;
	struct inode_info *inode;

	data = extend_read(vbfs_ctx.inode_queue, ino, &ebuf);
	if (IS_ERR(data)) {
		ret = PTR_ERR(data);
		goto err;
	}

	inode = find_inode(ebuf);
	if (NULL == inode_v) {
		ret = -ENOMEM;
		goto err;
	}

	extend_put(ebuf);

	return 0;
}

struct inode_info *pathname_to_inode(const char *pathname)
{
	int ret;
	struct inode_info *inode, *inode_tmp;
	char *name = NULL, *pos = NULL, *subname = NULL;

	name = strdup(pathname);
	if (NULL == name) {
		ret = -ENOMEM;
		return ERR_PTR(ret);
	}
	pos = name;

	inode = get_root_inode();

	while ((subname = pathname_str_sep(&pos, PATH_SEP)) != NULL) {
		if (strlen(subname) == 0)
			continue;

		inode_tmp = inode_lookup_by_name(inode, subname);
		if (IS_ERR(inode_tmp)) {
			ret = PTR_ERR(inode_tmp);
			free(name);
			return ERR_PTR(ret);
		}
		inode = inode_tmp;
		inode_tmp = NULL;

		vbfs_inode_close(inode);
	}

	free(name);

	return inode;
}
