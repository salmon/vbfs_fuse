#include "utils.h"
#include "log.h"
#include "mempool.h"
#include "vbfs-fuse.h"
#include "inode.h"

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

	log_dbg("\n");
	log_dbg("group_no %u\n", dir_info->group_no);
	log_dbg("total_extends %u\n", dir_info->total_extends);
	log_dbg("bitmap_size %u\n", dir_info->bitmap_size);
	log_dbg("dir_self_count %u\n", dir_info->dir_self_count);
	log_dbg("dir_total_count %u\n", dir_info->dir_total_count);
	log_dbg("\n");
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

	log_dbg("ino %u\n", dir->inode);
	log_dbg("file type %u\n", dir->file_type);
	log_dbg("name %s\n", dir->name);
}

static void save_dirent(struct vbfs_dirent_disk * dir_dk, struct dentry_vbfs *dir)
{
	dir_dk->inode = le32_to_cpu(dir->inode);
	dir_dk->file_type = le32_to_cpu(dir->file_type);

	memcpy(dir_dk->name, dir->name, NAME_LEN);
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

	bitmap_bits = dir_info->bitmap_size * 4096;

	log_dbg("##### dir extend info %u #####\n", dir_info->dir_self_count);

	for (i = 0; i < dir_info->dir_self_count; i ++) {
		offset = bitops_next_pos(bitmap, bitmap_bits, offset);
		if (offset == 0) {
			break;
		}
		dir = mp_malloc(sizeof(struct dentry_vbfs));

		pos = extend + (offset - 1) * VBFS_DIR_SIZE;
		load_dirent((struct vbfs_dirent_disk *) pos, dir);

		list_add(&dir->dentry_list, dir_list);
	}

	log_dbg("get_dentry_in_extend EXIT\n");

	return ret;
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

	ret = inode_get_first_extend_unlocked(inode_v);
	if (ret) {
		pthread_mutex_unlock(&inode_v->lock_inode);
		return ret;
	}

	load_dentry_info((vbfs_dir_meta_dk_t *) inode_v->inode_first_ext, &fst_dir_info);

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

int add_dentry()
{
	return 0;
}

void fill_stbuf_by_inode(struct stat *stbuf, struct inode_vbfs *inode_v)
{
	stbuf->st_ino = inode_v->i_ino;

	if (inode_v->i_mode == VBFS_FT_DIR) {
		stbuf->st_mode = S_IFDIR | 0777;
	} else if (inode_v->i_mode == VBFS_FT_REG_FILE) {
		stbuf->st_mode = S_IFREG | 0777;
	}

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

		filler(filler_buf, dentry->name, &st, 0);
	}

	put_dentry(&dir_list);

	return 0;
}
