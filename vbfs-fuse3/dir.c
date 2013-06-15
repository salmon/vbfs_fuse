#include "utils.h"
#include "log.h"
#include "mempool.h"
#include "vbfs-fuse.h"

static void load_dirent_info(vbfs_dir_meta_dk_t *dir_meta_disk,
				struct dentry_info *dir_info)
{
	dir_info->group_no = le32_to_cpu(dir_meta_disk->vbfs_dir_meta.group_no);
	dir_info->total_extends = le32_to_cpu(dir_meta_disk->vbfs_dir_meta.total_extends);

	dir_info->dir_self_count = le32_to_cpu(dir_meta_disk->vbfs_dir_meta.dir_self_count);
	dir_info->dir_total_count = le32_to_cpu(dir_meta_disk->vbfs_dir_meta.dir_total_count);

	dir_info->next_extend = le32_to_cpu(dir_meta_disk->vbfs_dir_meta.next_extend);
	dir_info->dir_capacity = le32_to_cpu(dir_meta_disk->vbfs_dir_meta.dir_capacity);
	dir_info->bitmap_size = le32_to_cpu(dir_meta_disk->vbfs_dir_meta.bitmap_size);
}

static void save_dirent_info(vbfs_dir_meta_dk_t *dir_meta_disk,
				struct dentry_info *dir_info)
{
	memset(dir_meta_disk, 0, sizeof(vbfs_dir_meta_dk_t));

	dir_meta_disk->vbfs_dir_meta.group_no = cpu_to_le32(dir_info->group_no);
	dir_meta_disk->vbfs_dir_meta.total_extends = cpu_to_le32(dir_info->total_extends);

	dir_meta_disk->vbfs_dir_meta.dir_self_count = cpu_to_le32(dir_info->dir_self_count);
	dir_meta_disk->vbfs_dir_meta.dir_total_count = cpu_to_le32(dir_info->dir_total_count);

	dir_meta_disk->vbfs_dir_meta.next_extend = cpu_to_le32(dir_info->next_extend);
	dir_meta_disk->vbfs_dir_meta.dir_capacity = cpu_to_le32(dir_info->dir_capacity);
	dir_meta_disk->vbfs_dir_meta.bitmap_size = cpu_to_le32(dir_info->bitmap_size);
}

static void load_dirent(struct vbfs_dirent_disk * dir_dk, struct dentry_vbfs *dir)
{
	dir->inode = cpu_to_le32(dir_dk->inode);
	dir->file_type = cpu_to_le32(dir_dk->file_type);

	memcpy(dir->name, dir_dk->name, NAME_LEN);
}

static void save_dirent(struct vbfs_dirent_disk * dir_dk, struct dentry_vbfs *dir)
{
	dir_dk->inode = le32_to_cpu(dir->inode);
	dir_dk->file_type = le32_to_cpu(dir->file_type);

	strncpy(dir_dk->name, dir->name, NAME_LEN);
}

void init_inode_dirent(struct inode_dirents *dirent)
{
	dirent->ref = 0;
	dirent->dir_cnt = 0;
	dirent->status = DIR_NOT_READY;
	INIT_LIST_HEAD(&dirent->dir_list);
}

static int get_dirent_by_edata(struct dentry_info *dir_info,
			struct extend_data *edata, struct inode_dirents *dirent)
{
	int i;
	char *pos = NULL, *dir_offset = NULL;
	struct dentry_vbfs *dir = NULL;

	if (BUFFER_NOT_READY == edata->status) {
		log_err("BUG");
		return -1;
	}

	pos = edata->buf;

	pthread_mutex_lock(&edata->ed_lock);
	load_dirent_info((vbfs_dir_meta_dk_t *) pos, dir_info);

	dir_offset = edata->buf + VBFS_DIR_META_SIZE
			+ dir_info->bitmap_size * VBFS_DIR_SIZE;

	for (i = 0; i < dir_info->dir_self_count; i ++) {
		dir = mp_malloc(sizeof(struct dentry_vbfs));
		if (NULL == dir) {
			pthread_mutex_unlock(&edata->ed_lock);
			return -ENOMEM;
		}

		pos = dir_offset + i * VBFS_DIR_SIZE;
		load_dirent((struct vbfs_dirent_disk *) pos, dir);

		dirent->dir_cnt ++;
		list_add(&dir->dentry_list, &dirent->dir_list);
	}
	pthread_mutex_unlock(&edata->ed_lock);

	return 0;
}

static int get_dirent_by_extendno(const __u32 extend_no, struct dentry_info *dir_info,
				struct inode_vbfs *inode_v, struct inode_dirents *dirent)
{
	int ret = 0;
	struct extend_data *edata = NULL;

	edata = get_edata_by_inode_unlocked(extend_no, inode_v, &ret);
	if (ret)
		return ret;

	ret = get_dirent_by_edata(dir_info, edata, dirent);
	if (ret) 
		return ret;

	ret = put_edata_by_inode_unlocked(extend_no, inode_v);
	if (ret)
		return ret;

	return ret;
}

static int get_dentry_unlocked(struct inode_vbfs *inode_v)
{
	struct dentry_info dir_info;
	__u32 extend_no = 0;
	int i, ext_num, ret = 0;
	struct inode_dirents *dirent = NULL;

	memset(&dir_info, 0, sizeof(dir_info));
	if (! (inode_v->i_mode == VBFS_FT_DIR)) {
		return -ENOTDIR;
	}

	if (DIR_NOT_READY != inode_v->dirent.status)
		return 0;

	dirent = &inode_v->dirent;
	extend_no = inode_v->i_extend;

	ret = get_dirent_by_extendno(extend_no, &dir_info, inode_v, dirent);
	if (ret)
		return ret;

	ext_num = dir_info.total_extends;
	for (i = 1; i < ext_num; i ++) {
		extend_no = dir_info.next_extend;
		if (0 != extend_no) {
			ret = get_dirent_by_extendno(extend_no, &dir_info, inode_v, dirent);
			if (ret)
				return ret;
		} else {
			log_err("BUG");
			return -1;
		}
	}

	dirent->status = DIR_CLEAN;

	return 0;
}

static int put_dirent_list(struct list_head *dir_list)
{
	struct dentry_vbfs *dentry = NULL;
	struct dentry_vbfs *tmp = NULL;

	list_for_each_entry_safe(dentry, tmp, dir_list, dentry_list) {
		list_del(&dentry->dentry_list);
		mp_free(dentry);
	}

	INIT_LIST_HEAD(dir_list);

	return 0;
}

int get_dentry(struct inode_vbfs *inode_v)
{
	int ret = 0;

	pthread_mutex_lock(&inode_v->inode_lock);
	ret = get_dentry_unlocked(inode_v);
	if (ret) {
		put_dirent_list(&inode_v->dirent.dir_list);
		init_inode_dirent(&inode_v->dirent);
	}
	pthread_mutex_unlock(&inode_v->inode_lock);

	return ret;
}

static void dirent_info_src_to_dest(struct dentry_info *src, struct dentry_info *dest)
{
	dest->total_extends = src->total_extends;
	dest->dir_total_count = src->dir_total_count;
	dest->dir_capacity = src->dir_capacity;
	dest->bitmap_size = src->bitmap_size;
}

static int writeback_dirent_unlocked(struct inode_vbfs *inode_v)
{
	int ret = 0, dir_offset = 0, i = 0, total_extends = 0;
	char *pos = NULL;
	__u32 capacity = 0;

	struct inode_dirents *dirent = NULL;
	struct dentry_vbfs *dir = NULL;
	struct extend_data *edata = NULL;
	struct dentry_info dir_info;

	dirent = &inode_v->dirent;

	if (DIR_DIRTY != dirent->status)
		return 0;

	log_dbg("i_extend %u", inode_v->i_extend);
	edata = get_edata_by_inode_unlocked(inode_v->i_extend, inode_v, &ret);
	if (ret)
		return ret;

	if (BUFFER_NOT_READY == edata->status) {
		log_err("BUG");
		return -1;
	}

	pos = edata->buf;

	pthread_mutex_lock(&edata->ed_lock);
	load_dirent_info((vbfs_dir_meta_dk_t *) pos, &dir_info);
	pthread_mutex_unlock(&edata->ed_lock);

	capacity = dir_info.dir_capacity;
	dir_offset = VBFS_DIR_META_SIZE + dir_info.bitmap_size * VBFS_DIR_SIZE;
	if (dirent->dir_cnt % capacity)
		total_extends = dirent->dir_cnt / capacity + 1;
	else
		total_extends = dirent->dir_cnt / capacity;

	dir_info.dir_total_count = dirent->dir_cnt;

	if (1 == total_extends) {
		pthread_mutex_lock(&edata->ed_lock);

		list_for_each_entry(dir, &dirent->dir_list, dentry_list) {
			pos = edata->buf + dir_offset + (i % capacity) * VBFS_DIR_SIZE;
			save_dirent((struct vbfs_dirent_disk *) pos, dir);
			++i;
		}
		dir_info.dir_self_count = i;
		pos = edata->buf;
		save_dirent_info((vbfs_dir_meta_dk_t *) pos, &dir_info);
		edata->status = BUFFER_DIRTY;

		pthread_mutex_unlock(&edata->ed_lock);
		log_err("%u %u %u", inode_v->i_extend, inode_v->i_ino, edata->extend_no);
		put_edata_by_inode_unlocked(inode_v->i_extend, inode_v);

		dirent->status = DIR_CLEAN;

		return 0;
	}

	return 0;
}

int put_dentry_unlocked(struct inode_vbfs *inode_v)
{
	int ret = 0;

	if (VBFS_FT_DIR != inode_v->i_mode) {
		return -ENOTDIR;
	}

	ret = writeback_dirent_unlocked(inode_v);
	if (ret)
		return ret;

	put_dirent_list(&inode_v->dirent.dir_list);
	init_inode_dirent(&inode_v->dirent);

	return 0;
}

int put_dentry(struct inode_vbfs *inode_v)
{
	int ret = 0;

	pthread_mutex_lock(&inode_v->inode_lock);
	ret = put_dentry_unlocked(inode_v);
	pthread_mutex_unlock(&inode_v->inode_lock);

	return ret;
}

int sync_dentry(struct inode_vbfs *inode_v)
{
	int ret = 0;

	pthread_mutex_lock(&inode_v->inode_lock);
	ret = writeback_dirent_unlocked(inode_v);
	pthread_mutex_unlock(&inode_v->inode_lock);

	return ret;
}

void fill_stbuf_by_inode(struct stat *stbuf, struct inode_vbfs *inode_v)
{
	stbuf->st_ino = inode_v->i_ino;

	if (inode_v->i_mode == VBFS_FT_DIR) {
		stbuf->st_mode = S_IFDIR | 0777;
	} else if (inode_v->i_mode == VBFS_FT_REG_FILE) {
		stbuf->st_mode = S_IFREG | 0777;
	}

	stbuf->st_size = inode_v->i_size;

	stbuf->st_atime = inode_v->i_atime;
	stbuf->st_mtime = inode_v->i_mtime;
	stbuf->st_ctime = inode_v->i_ctime;
}

int vbfs_readdir(struct inode_vbfs *inode_v, off_t *filler_pos,
			fuse_fill_dir_t filler, void *filler_buf)
{
	struct inode_dirents *dirent = NULL;
	struct dentry_vbfs *dentry = NULL;
	int ret = 0;
	struct stat st;

	ret = get_dentry(inode_v);
	if (ret)
		return ret;

	dirent = &inode_v->dirent;

	list_for_each_entry(dentry, &dirent->dir_list, dentry_list) {
		memset(&st, 0, sizeof(st));

		inode_v = vbfs_inode_open(dentry->inode, &ret);
		if (ret)
			return ret;

		fill_stbuf_by_inode(&st, inode_v);

		filler(filler_buf, dentry->name, &st, 0);
	}

	return 0;
}

static int add_dir_to_list(struct inode_vbfs *v_inode_parent,
			struct inode_vbfs *inode_v, const char *name)
{
	struct inode_dirents *dirent = NULL;
	struct dentry_vbfs *dir = NULL;

	dirent = &v_inode_parent->dirent;

	list_for_each_entry(dir, &dirent->dir_list, dentry_list) {
		if (0 == strncmp(dir->name, name, NAME_LEN - 1)) {
			return -EEXIST;
		}
	}

	dir = mp_malloc(sizeof(struct dentry_vbfs));
	if (NULL == dir)
		return -ENOMEM;

	dir->inode = inode_v->i_ino;
	dir->file_type = inode_v->i_mode;
	strncpy(dir->name, name, NAME_LEN - 1);

	list_add_tail(&dir->dentry_list, &dirent->dir_list);
	dirent->dir_cnt ++;
	dirent->status = DIR_DIRTY;

	return 0;
}

static void get_dir_bitmap_capacity(__u32 *capacity, __u32 *bm_size)
{
	__u32 dir_count = 0;
	__u32 bitmap_size = 0;

	dir_count = (get_extend_size() - VBFS_DIR_META_SIZE) / VBFS_DIR_SIZE;

	if (dir_count % 4096)
		bitmap_size = dir_count / 4096 + 1;
	else
		bitmap_size = dir_count / 4096;

	*bm_size = bitmap_size;
	*capacity = dir_count - bitmap_size;
}

static void init_default_dir_info(struct dentry_info *dir_info)
{
	__u32 bm_size = 0, cpcity = 0;

	get_dir_bitmap_capacity(&cpcity, &bm_size);

	dir_info->group_no = 0;
	dir_info->total_extends = 0;
	dir_info->dir_self_count = 2;
	dir_info->dir_total_count = 2;
	dir_info->next_extend = 0;

	dir_info->dir_capacity = cpcity;
	dir_info->bitmap_size = bm_size;
}

int dir_init_default_fst(struct inode_vbfs *inode_v)
{
	struct dentry_info dir_info;
	struct dentry_vbfs *dir[2];
	struct extend_data *edata = NULL;
	char *pos = NULL, *dir_offset = NULL;
	int ret = 0;

	init_default_dir_info(&dir_info);
	memset(&dir, 0, sizeof(dir));

	edata = alloc_edata_by_inode(inode_v->i_extend, inode_v, &ret);
	if (ret)
		return ret;

	if (BUFFER_NOT_READY == edata->status) {
		log_err("BUG");
		return -1;
	}

	dir[0] = mp_malloc(sizeof(struct dentry_vbfs));
	if (NULL == dir[0]) {
		put_edata_by_inode(inode_v->i_extend, inode_v);
		return -ENOMEM;
	}
	dir[1] = mp_malloc(sizeof(struct dentry_vbfs));
	if (NULL == dir[1]) {
		free(dir[0]);
		put_edata_by_inode(inode_v->i_extend, inode_v);
		return -ENOMEM;
	}

	dir[0]->inode = inode_v->i_ino;
	dir[0]->file_type = VBFS_FT_DIR;
	strncpy(dir[0]->name, ".", NAME_LEN - 1);
	list_add(&dir[0]->dentry_list, &inode_v->dirent.dir_list);

	dir[1]->inode = inode_v->i_pino;
	dir[1]->file_type = VBFS_FT_DIR;
	strncpy(dir[1]->name, "..", NAME_LEN - 1);
	list_add(&dir[1]->dentry_list, &inode_v->dirent.dir_list);

	pthread_mutex_lock(&edata->ed_lock);

	pos = edata->buf;
	save_dirent_info((vbfs_dir_meta_dk_t *) pos, &dir_info);

	dir_offset = edata->buf + VBFS_DIR_META_SIZE
			+ dir_info.bitmap_size * VBFS_DIR_SIZE;

	pos = dir_offset;
	save_dirent((struct vbfs_dirent_disk *) pos, dir[0]);

	pos = dir_offset + VBFS_DIR_SIZE;
	save_dirent((struct vbfs_dirent_disk *) pos, dir[1]);

	pthread_mutex_unlock(&edata->ed_lock);

	put_edata_by_inode(inode_v->i_extend, inode_v);

	return 0;
}

static int add_dirent_unlocked(struct inode_vbfs *v_inode_parent,
			struct inode_vbfs *inode_v, const char *name)
{
	int ret = 0;

 	ret = add_dir_to_list(v_inode_parent, inode_v, name);
	if (ret)
		return ret;

	ret = writeback_dirent_unlocked(v_inode_parent);
	if (ret)
		return ret;

	return 0;
} 

int add_dirent(struct inode_vbfs *v_inode_parent,
			struct inode_vbfs *inode_v, const char *name)
{
	int ret = 0;

	pthread_mutex_lock(&inode_v->inode_lock);
	ret = add_dirent_unlocked(v_inode_parent, inode_v, name);
	pthread_mutex_unlock(&inode_v->inode_lock);

	return ret;
}

int vbfs_mkdir(struct inode_vbfs *v_inode_parent, const char *name)
{
	int ret = 0;
	struct inode_vbfs *inode_v = NULL;

	inode_v = alloc_inode(v_inode_parent->i_ino, VBFS_FT_DIR, &ret);
	if (ret)
		return ret;

	ret = add_dirent(v_inode_parent, inode_v, name);
	if (ret) {
		vbfs_inode_close(inode_v);
		return ret;
	}

	vbfs_inode_close(inode_v);

	return 0;
}

