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
 *
 * */
static struct inode_vbfs *open_inode(__u32 ino, int *err_no)
{
	struct extend_data *edata = NULL;
	struct inode_vbfs *inode_v = NULL;
	__u32 extend_no = 0, inode_offset = 0;
	char *pos = NULL;

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

	read_edata(edata);
	if (BUFFER_NOT_READY != edata->status) {
		pthread_mutex_lock(&edata->ed_lock);

		pos = edata->buf + inode_offset;
		load_inode_info(inode_v, (vbfs_inode_dk_t *) pos);

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

	return inode_v;

destroy_edata:
	close_edata(edata);
	mp_free(inode_v);

err:
	return NULL;
}

static int writeback_inode(struct inode_vbfs *inode_v)
{
	struct extend_data *edata = NULL;
	__u32 extend_no = 0, inode_offset = 0;
	int ret = 0;
	char *pos = NULL;

	if (get_inode_position(inode_v->i_ino, &extend_no, &inode_offset))
		return -INTERNAL_ERR;

	edata = open_edata(extend_no, &vbfs_ctx.inode_queue, &ret);
	if (ret)
		goto err;

	read_edata(edata);
	if (BUFFER_NOT_READY != edata->status) {
		pthread_mutex_lock(&edata->ed_lock);

		pos = edata->buf + inode_offset;

		save_inode_info(inode_v, (vbfs_inode_dk_t *) pos);
		edata->status = BUFFER_DIRTY;

		pthread_mutex_unlock(&edata->ed_lock);
	} else {
		ret = -EIO;
		goto destroy_edata;
	}

	close_edata(edata);

	inode_v->inode_dirty = INODE_CLEAN;

	return 0;

destroy_edata:
	close_edata(edata);

err:
	return ret;
}

/*
 * May not need active_inode_lock ?
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

	inode_v = open_inode(ROOT_INO, &ret);
	if (ret) {
		return ret;
	}

	list_add_tail(&inode_v->active_list, &vbfs_ctx.active_inode_list);

	return 0;
}

static struct inode_vbfs *get_active_inode(const __u32 ino)
{
	struct inode_vbfs *inode_v = NULL;

	list_for_each_entry(inode_v, &vbfs_ctx.active_inode_list, active_list) {
		if (inode_v->i_ino == ino)
			return inode_v;
	}

	return NULL;
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

struct inode_vbfs *vbfs_inode_open(__u32 ino, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;

	if (ROOT_INO == ino) {
		return get_root_inode();
	}

	pthread_mutex_lock(&vbfs_ctx.active_inode_lock);
	inode_v = vbfs_inode_open_unlocked(ino, err_no);
	pthread_mutex_unlock(&vbfs_ctx.active_inode_lock);

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

static int init_default_fst_extend(struct inode_vbfs *inode_v)
{
	int ret = 0;

	if (inode_v->i_mode == VBFS_FT_DIR) {
		/* init default dirent */
		ret = dir_init_default_fst(inode_v);
	} else {
		/* init default file idx */
		ret = file_init_default_fst(inode_v);
	}

	return ret;
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

int vbfs_inode_update_times(struct inode_vbfs *v_inode, time_update_flags mask)
{
	return 0;
}

int vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode_parent, const char *name, __u32 *ino)
{
	struct inode_dirents *dirs = NULL;
	int ret = 0;
	struct dentry_vbfs *dentry = NULL;
	int found = 0;

	ret = get_dentry(v_inode_parent);
	if (ret)
		return ret;

	/* processing */
	dirs = &v_inode_parent->dirent;
	if (dirs->status == DIR_NOT_READY) {
		log_err("BUG");
		return -1;
	}

	list_for_each_entry(dentry, &dirs->dir_list, dentry_list) {
		if (0 == strncmp(dentry->name, name, NAME_LEN - 1)) {
			found = 1;
			*ino = dentry->inode;
			break;
		}
	}

	if (found) {
		return 0;
	} else {
		return -ENOENT;
	}

	return 0;
}

#if 0
/*
 * Is it necessary to take inode_lock ?
 * */
int vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode_parent, const char *name, __u32 *ino)
{
	int ret = 0;

	pthread_mutex_lock(&v_inode_parent->inode_lock);
	ret = vbfs_inode_lookup_unlocked(v_inode_parent, name, ino);
	pthread_mutex_unlock(&v_inode_parent->inode_lock);

	return ret;
}
#endif

struct inode_vbfs *vbfs_pathname_to_inode(const char *pathname, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;
	char *name = NULL, *pos = NULL, *subname = NULL;
	__u32 ino = 0;

	name = strdup(pathname);
	if (NULL == name) {
		*err_no = -ENOMEM;
	}
	pos = name;

	inode_v = get_root_inode();

	while ((subname = pathname_str_sep(&pos, PATH_SEP)) != NULL) {
		if (strlen(subname) == 0)
			continue;

		*err_no = vbfs_inode_lookup_by_name(inode_v, subname, &ino);
		vbfs_inode_close(inode_v);
		if (*err_no) {
			free(name);
			return NULL;
		}

		inode_v = vbfs_inode_open(ino, err_no);
		if (*err_no) {
			free(name);
			return NULL;
		}
	}

	free(name);

	return inode_v;
}
