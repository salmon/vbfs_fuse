#include "utils.h"
#include "log.h"
#include "mempool.h"
#include "vbfs-fuse.h"
#include "inode.h"

extern vbfs_fuse_context_t vbfs_ctx;

static void load_dentry_info(vbfs_dir_meta_dk_t *dir_meta_disk,
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

static void save_dentry_info(vbfs_dir_meta_dk_t *dir_meta_disk,
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

static int get_dentry_in_extend(struct dentry_info *dir_info,
				char *extend, struct list_head *dir_list)
{
	int ret = 0, i = 0;
	__u32 offset = 0;
	char *bitmap = NULL;
	__u32 bitmap_bits = 0;
	struct dentry_vbfs *dir = NULL;
	char *pos = NULL;
	char *dir_data = 0;

	bitmap = dir_info->dentry_bitmap;
	bitmap_bits = dir_info->bitmap_size * 4096;
	dir_data = extend + VBFS_DIR_META_SIZE + dir_info->bitmap_size * VBFS_DIR_SIZE;

	log_dbg("##### dir extend info %u #####\n", dir_info->dir_self_count);

	for (i = 0; i < dir_info->dir_self_count; i ++) {
		offset = bitops_next_pos_set(bitmap, bitmap_bits, offset);
		if (offset == 0) {
			break;
		}
		dir = mp_malloc(sizeof(struct dentry_vbfs));

		pos = dir_data + (offset - 1) * VBFS_DIR_SIZE;
		load_dirent((struct vbfs_dirent_disk *) pos, dir);

		list_add(&dir->dentry_list, dir_list);
	}

	return ret;
}

static int get_fst_dirent_info(struct inode_vbfs *inode_v, struct dentry_info *fst_dir_info)
{
	int ret = 0;

	ret = inode_get_first_extend_unlocked(inode_v);
	if (ret) {
		return ret;
	}

	load_dentry_info((vbfs_dir_meta_dk_t *) inode_v->inode_first_ext, fst_dir_info);
	fst_dir_info->dentry_bitmap = inode_v->inode_first_ext + VBFS_DIR_META_SIZE;

	return 0;
}

int get_dentry(struct inode_vbfs *inode_v, struct list_head *dir_list)
{
	struct dentry_info fst_dir_info;
	int ret = 0;

	pthread_mutex_lock(&inode_v->lock_inode);

	if (! (inode_v->i_mode | VBFS_FT_DIR)) {
		pthread_mutex_unlock(&inode_v->lock_inode);
		return -ENOTDIR;
	}

	ret = get_fst_dirent_info(inode_v, &fst_dir_info);
	if (ret) {
		pthread_mutex_unlock(&inode_v->lock_inode);
		return ret;
	}

	ret = get_dentry_in_extend(&fst_dir_info, inode_v->inode_first_ext, dir_list);
	if (ret) {
		pthread_mutex_unlock(&inode_v->lock_inode);
		return ret;
	}

	if (fst_dir_info.total_extends > 1) {
		/* Not Implement that dir use more than one extend */
		log_err("Not Implement\n");
	}

	pthread_mutex_unlock(&inode_v->lock_inode);

	return 0;
}

int put_dentry(struct list_head *dir_list)
{
	struct dentry_vbfs *dentry = NULL;
	struct dentry_vbfs *tmp = NULL;

	list_for_each_entry_safe(dentry, tmp, dir_list, dentry_list) {
		list_del(&dentry->dentry_list);
		mp_free(dentry, sizeof(struct dentry_vbfs));
	}

	INIT_LIST_HEAD(dir_list);

	return 0;
}

static __u32 get_dir_bitmap_size()
{
	__u32 extend_size = 0;
	__u32 dir_count = 0;
	__u32 bitmap_size = 0;

	extend_size = vbfs_ctx.super.s_extend_size;
	dir_count = (extend_size - VBFS_DIR_META_SIZE) / VBFS_DIR_SIZE;

	if (dir_count % 4096) {
		bitmap_size = dir_count / 4096 + 1;
	} else {
		bitmap_size = dir_count / 4096;
	}

	return bitmap_size;
}

static __u32 get_dir_bitmap_capacity()
{
	__u32 extend_size = 0;
	__u32 dir_count = 0;
	__u32 bitmap_size = 0;
	__u32 dir_capacity = 0;

	extend_size = vbfs_ctx.super.s_extend_size;
	dir_count = (extend_size - VBFS_DIR_META_SIZE) / VBFS_DIR_SIZE;

	if (dir_count % 4096)
		bitmap_size = dir_count / 4096 + 1;
	else
		bitmap_size = dir_count / 4096;

	dir_capacity = dir_count - bitmap_size;

	return dir_capacity;
}

void init_default_dir(struct inode_vbfs *inode_v, __u32 p_ino)
{
	__u32 bitmap_bits = 0;
	char *pos = NULL;
	struct dentry_info dir_info;
	char *bitmap = NULL;
	__u32 offset = 0;
	int i = 0;
	struct dentry_vbfs dir;

	dir_info.group_no = 0;
	dir_info.total_extends = 1;

	dir_info.dir_self_count = 2;
	dir_info.dir_total_count = 2;

	dir_info.next_extend = 0;
	dir_info.bitmap_size = get_dir_bitmap_size();
	dir_info.dir_capacity = get_dir_bitmap_capacity();

	pos = inode_v->inode_first_ext;
	save_dentry_info((vbfs_dir_meta_dk_t *) pos, &dir_info);

	/* init bitmap */
	bitmap = inode_v->inode_first_ext + VBFS_DIR_META_SIZE;
	bitmap_bits = dir_info.bitmap_size * 4096;

	for (i = 0; i < dir_info.dir_self_count; i ++) {
		offset = find_zerobit_and_set(bitmap, bitmap_bits, 0);
		if (offset != (i + 1))
			log_err("BUG\n");
	}

	/* init default dirent(.)(..) */
	pos = inode_v->inode_first_ext + VBFS_DIR_META_SIZE + dir_info.bitmap_size * VBFS_DIR_SIZE;
	memset(&dir, 0, sizeof(dir));
	dir.inode = inode_v->i_ino;
	dir.file_type = VBFS_FT_DIR;
	strcpy(dir.name, ".");
	save_dirent((struct vbfs_dirent_disk *) pos, &dir);

	pos += VBFS_DIR_SIZE;
	memset(&dir, 0, sizeof(dir));
	dir.inode = p_ino;
	dir.file_type = VBFS_FT_DIR;
	strcpy(dir.name, "..");
	save_dirent((struct vbfs_dirent_disk *) pos, &dir);
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

int vbfs_readdir(struct inode_vbfs *inode_v, off_t *filler_pos, fuse_fill_dir_t filler, void *filler_buf)
{
	struct list_head dir_list;
	int ret = 0;
	struct dentry_vbfs *dentry = NULL;
	struct stat st;

	INIT_LIST_HEAD(&dir_list);

	ret = get_dentry(inode_v, &dir_list);
	if (ret)
		return ret;

	list_for_each_entry(dentry, &dir_list, dentry_list) {
		memset(&st, 0, sizeof(st));

		inode_v = vbfs_inode_open(dentry->inode, &ret);
		if (ret)
			return ret;

		fill_stbuf_by_inode(&st, inode_v);
		log_dbg("ino %u", st.st_ino);

		filler(filler_buf, dentry->name, &st, 0);
	}

	put_dentry(&dir_list);

	return 0;
}

int add_dirent(struct inode_vbfs *inode_v, const char *name, __u32 ino)
{
	struct dentry_info fst_dir_info;
	struct dentry_vbfs dir;
	int ret = 0;
	__u32 bitmap_bits = 0;
	__u32 offset = 0;
	char *bitmap = NULL, *pos = NULL;

	memset(&dir, 0, sizeof(dir));

	pthread_mutex_lock(&inode_v->lock_inode);

	if (inode_v->i_mode != VBFS_FT_DIR) {
		pthread_mutex_unlock(&inode_v->lock_inode);
		return -ENOTDIR;
	}

	ret = get_fst_dirent_info(inode_v, &fst_dir_info);
	if (ret) {
		pthread_mutex_unlock(&inode_v->lock_inode);
		return ret;
	}

	bitmap_bits = fst_dir_info.bitmap_size * 4096;
	fst_dir_info.dir_self_count ++;
	fst_dir_info.dir_total_count ++;

	dir.inode = ino;
	dir.file_type = VBFS_FT_DIR;
	strncpy(dir.name, name, NAME_LEN);

	bitmap = fst_dir_info.dentry_bitmap;

	offset = find_zerobit_and_set(bitmap, bitmap_bits, 0);
	if (offset != 0) {
		pos =  inode_v->inode_first_ext + VBFS_DIR_META_SIZE
			+ (offset + fst_dir_info.bitmap_size - 1) * VBFS_DIR_SIZE;
		save_dirent((struct vbfs_dirent_disk *) pos, &dir);

		pos = inode_v->inode_first_ext;
		save_dentry_info((vbfs_dir_meta_dk_t *) pos, &fst_dir_info);

		inode_v->first_ext_status = 2;
	} else {
		/* Not Implement */
		log_err("Not Implement");
	}


	pthread_mutex_unlock(&inode_v->lock_inode);

	return 0;
}

int vbfs_mkdir(struct inode_vbfs *v_inode_parent, const char *name)
{
	int ret = 0;
	struct inode_vbfs *inode_v = NULL;

	inode_v = vbfs_inode_create(v_inode_parent->i_ino, VBFS_FT_DIR, &ret);
	if (ret) {
		return ret;
	}

	ret = add_dirent(v_inode_parent, name, inode_v->i_ino);
	if (ret) {
		vbfs_inode_close(inode_v);
		return ret;
	}

	vbfs_inode_close(inode_v);

	return 0;
}
