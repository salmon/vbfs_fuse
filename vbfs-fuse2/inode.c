#include "utils.h"
#include "log.h"
#include "mempool.h"
#include "vbfs-fuse.h"
#include "dir.h"

extern vbfs_fuse_context_t vbfs_ctx;

static int alloc_extend_bitmap(__u32 *extend_no)
{
	__u32 extend_off_t = 0;

	extend_off_t = vbfs_ctx.super.extend_bitmap_current;

	return 0;
}

static int alloc_inode_bitmap(__u32 *inode_no)
{
	return 0;
}

static int free_extend_bitmap()
{
	return 0;
}

static int free_inode_bitmap()
{
	return 0;
}

int vbfs_inode_update_times(struct inode_vbfs *v_inode, time_update_flags mask)
{
	if (mask | UPDATE_ATIME)
		v_inode->i_atime = time(NULL);

	if (mask | UPDATE_MTIME)
		v_inode->i_mtime = time(NULL);

	vbfs_inode_mark_dirty(v_inode);

	return 0;
}

static struct inode_vbfs *check_inode_opened(__u32 ino)
{
	struct inode_vbfs *inode_v = NULL;

	list_for_each_entry(inode_v, &vbfs_ctx.active_inode_list, inode_l) {
		if (inode_v->i_ino == ino)
			return inode_v;
	}

	return NULL;
}

static void add_to_active_inodelist(struct inode_vbfs *inode_v)
{
	list_add_tail(&inode_v->inode_l, &vbfs_ctx.active_inode_list);
}

static int get_ino_bmc_info(struct inode_bitmap_cache *ino_bmc, __u32 group_no)
{
	__u32 extend_size = 0;
	__u32 extend_no = 0;
	char *extend_buf = NULL;

	if (ino_bmc->cache_status) {
		return 0;
	}

	extend_size = vbfs_ctx.super.s_extend_size;

	if ((extend_buf = mp_valloc(extend_size)) == NULL) {
		return -ENOMEM;
	}

	extend_no = vbfs_ctx.super.inode_bitmap_offset + group_no;
	if (read_extend(extend_no, extend_buf)) {
		mp_free(extend_buf, extend_size);
		return -EIO;
	}

	ino_bmc->cache_status = 1;
	ino_bmc->content = extend_buf;
	ino_bmc->inode_bitmap_region = ino_bmc->content + INODE_BITMAP_META_SIZE;

	/* if exceed threshold, swap out other cache */
	/* Not Implement */

	return 0;
}

static int check_inode_valid_in_bitmap(__u32 ino)
{
	__u32 inode_bmc_no = 0;
	__u32 bit = 0;
	struct inode_bitmap_cache *ino_bmc = NULL;
	char *bitmap = NULL;
	__u32 bitmap_bits = 0;
	__u32 extend_size = 0;
	int ret = 0;

	extend_size = vbfs_ctx.super.s_extend_size;
	bitmap_bits = (extend_size - INODE_BITMAP_META_SIZE) * 8;

	inode_bmc_no = ino / bitmap_bits;
	bit = ino % bitmap_bits;

	ino_bmc = &vbfs_ctx.inode_bitmap_array[inode_bmc_no];

	pthread_mutex_lock(&ino_bmc->lock_ino_bm_cache);

	ret = get_ino_bmc_info(ino_bmc, inode_bmc_no);
	if (ret) {
		log_err("get_ino_bmc_info error\n");
		pthread_mutex_unlock(&ino_bmc->lock_ino_bm_cache);
		return ret;
	}

	bitmap = ino_bmc->inode_bitmap_region;
	ret = check_ffs(bitmap, bitmap_bits, bit);
	if (! ret) {
		pthread_mutex_unlock(&ino_bmc->lock_ino_bm_cache);
		return -ENOENT;
	}

	pthread_mutex_unlock(&ino_bmc->lock_ino_bm_cache);

	return 0;
}

struct inode_cache_in_ext *get_inode_cache(__u32 ino, __u32 *ino_off_in_ext, int *err_no)
{
	struct inode_cache_in_ext *inode_cache = NULL;
	char *extend_buf = NULL;
	__u32 extend_size = 0;
	__u32 extend_no = 0;
	__u32 inode_off_t = 0;
	__u32 inodes_per_extend = 0;

	log_dbg("get_inode_cache %u\n", ino);
	extend_size = vbfs_ctx.super.s_extend_size;
	inodes_per_extend = extend_size / INODE_SIZE;
	inode_off_t = ino / inodes_per_extend;
	*ino_off_in_ext = ino % inodes_per_extend;
	if (inode_off_t > vbfs_ctx.super.inode_extends) {
		log_err("BUG\n");
		*err_no = -EINVAL;
		return NULL;
	}
	extend_no = vbfs_ctx.super.inode_offset + inode_off_t;

	list_for_each_entry(inode_cache, &vbfs_ctx.inode_cache_list,
				ino_cache_in_ext_list) {
		if (inode_cache->extend_no == extend_no) {
			return inode_cache;
		}
	}

	if ((inode_cache = mp_malloc(sizeof(struct inode_cache_in_ext))) == NULL) {
		*err_no = -ENOMEM;
		return NULL;
	}

	if ((extend_buf = mp_valloc(extend_size)) == NULL) {
		mp_free(inode_cache, sizeof(struct inode_cache_in_ext));
		*err_no = -ENOMEM;
		return NULL;
	}

	if (read_extend(extend_no, extend_buf)) {
		mp_free(inode_cache, sizeof(struct inode_cache_in_ext));
		mp_free(extend_buf, extend_size);
		*err_no = -EIO;
		return NULL;
	}

	inode_cache->extend_no = extend_no;
	inode_cache->inode_cache_dirty = 0;
	inode_cache->content = extend_buf;

	log_dbg("inode cache status: %u, %p\n", inode_cache->extend_no, inode_cache->content);

	list_add_tail(&inode_cache->ino_cache_in_ext_list, &vbfs_ctx.inode_cache_list);
	vbfs_ctx.inode_cache_extend_cnt ++;

	/* if exceed threshold, swap out other cache */
	/* Not Implement */

	return inode_cache;
}

static void load_inode_info(vbfs_inode_dk_t *inode_disk, struct inode_vbfs *inode)
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

static struct inode_vbfs *open_inode(__u32 ino, int *err_no)
{
	struct inode_cache_in_ext *inode_cache;
	__u32 ino_off_in_ext = 0;
	char *pos = NULL;
	struct inode_vbfs *inode_v = NULL;

	log_dbg("open_inode %u, ENTER\n", ino);
	*err_no = 0;
	assert(vbfs_ctx.super.s_inode_count >= ino);

	if ((inode_v = mp_malloc(sizeof(struct inode_vbfs))) == NULL) {
		*err_no = -ENOMEM;
		return NULL;
	}

	pthread_mutex_lock(&vbfs_ctx.lock_inode_cache);

	inode_cache = get_inode_cache(ino, &ino_off_in_ext, err_no);
	if (*err_no) {
		log_err("get_inode_cache error %d\n", *err_no);
		mp_free(inode_v, sizeof(struct inode_vbfs));
		pthread_mutex_unlock(&vbfs_ctx.lock_inode_cache);
		return NULL;
	}

	log_dbg("cache extend_no is %u\n", inode_cache->extend_no);

	pos = inode_cache->content + INODE_SIZE * ino_off_in_ext;
	load_inode_info((vbfs_inode_dk_t *) pos, inode_v);

	pthread_mutex_unlock(&vbfs_ctx.lock_inode_cache);

	inode_v->inode_first_ext = NULL;
	inode_v->inode_dirty = 0;
	inode_v->first_ext_status = 0;
	inode_v->nref = 1;

	INIT_LIST_HEAD(&inode_v->data_buf_list);
	INIT_LIST_HEAD(&inode_v->inode_l);
	pthread_mutex_init(&inode_v->lock_inode, NULL);

	log_dbg("open_inode %u, EXIT\n", ino);

	return inode_v;
}

static struct inode_vbfs *get_vbfs_inode(__u32 ino, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;
	int ret = 0;

	log_dbg("get_vbfs_inode %u, ENTER\n", ino);

	ret = check_inode_valid_in_bitmap(ino);
	if (ret) {
		*err_no = ret;
		return NULL;
	}

	inode_v = open_inode(ino, err_no);
	if (*err_no) {
		return NULL;
	}

	log_dbg("get_vbfs_inode %u, EXIT\n", ino);

	return inode_v;
}

static struct inode_vbfs *vbfs_inode_open_unlocked(__u32 ino, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;

	log_dbg("vbfs_inode_open_unlocked ENTER\n");

	*err_no = 0;

	inode_v = check_inode_opened(ino);
	if (inode_v) {
		inode_v->nref ++;
		return inode_v;
	}

	inode_v = get_vbfs_inode(ino, err_no);
	if (*err_no) {
		return NULL;
	}

	add_to_active_inodelist(inode_v);

	log_dbg("vbfs_inode_open_unlocked EXIT\n");

	return inode_v;
}

struct inode_vbfs *vbfs_inode_open(__u32 ino, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;

	pthread_mutex_lock(&vbfs_ctx.lock_active_inode);
	inode_v = vbfs_inode_open_unlocked(ino, err_no);
	pthread_mutex_unlock(&vbfs_ctx.lock_active_inode);

	return inode_v;
}

struct inode_vbfs *vbfs_inode_create(__u32 ino, __u32 mode_t, int *err_no)
{
	return NULL;
}

static int inode_cache_wb_to_disk(struct inode_cache_in_ext *inode_cache)
{
	if (inode_cache->inode_cache_dirty) {
		if (write_extend(inode_cache->extend_no, inode_cache->content))
			return -EIO;
	}

	return 0;
}

static int inode_writeback_to_cache(struct inode_vbfs *i_vbfs)
{
	struct inode_cache_in_ext *inode_cache = NULL;
	__u32 ino_off_in_ext = 0;
	char *pos = NULL;
	int ret = 0;

	log_dbg("inode_writeback_to_cache ENTER\n");
	/* inode metadata sync */
	if (i_vbfs->inode_dirty) {
		inode_cache = get_inode_cache(i_vbfs->i_ino, &ino_off_in_ext, &ret);
		if (ret) {
			return ret;
		}
		pos = inode_cache->content + INODE_SIZE * ino_off_in_ext;
		save_inode_info(i_vbfs, (vbfs_inode_dk_t *) pos);
		inode_cache->inode_cache_dirty = 1;

		/* will low performance */
		ret = inode_cache_wb_to_disk(inode_cache);
		if (ret) {
			return ret;
		}
	}

	log_dbg("inode_writeback_to_cache EXIT\n");

	return 0;
}

static int vbfs_inode_sync_unlocked(struct inode_vbfs *i_vbfs)
{
	struct extend_content *extend_data = NULL;
	int ret = 0;

	log_err("vbfs_inode_sync_unlocked ENTER\n");

	pthread_mutex_lock(&vbfs_ctx.lock_inode_cache);
	ret = inode_writeback_to_cache(i_vbfs);
	pthread_mutex_unlock(&vbfs_ctx.lock_inode_cache);
	if (ret) {
		return ret;
	}

	/* inode data sync */
	if (i_vbfs->first_ext_status == 2) {
		if (write_extend(i_vbfs->i_extend, i_vbfs->inode_first_ext)) {
			ret = -EIO;
		}
	}

	list_for_each_entry(extend_data, &i_vbfs->data_buf_list, data_list) {
		if (extend_data->extend_dirty) {
			if (write_extend(extend_data->extend_no,
						extend_data->extend_buf)) {
				ret = -EIO;
			}
			extend_data->extend_dirty = 0;
		}
	}

	log_err("vbfs_inode_sync_unlocked EXIT\n");

	return ret;
}

int vbfs_inode_sync(struct inode_vbfs *i_vbfs)
{
	int ret = 0;

	pthread_mutex_lock(&i_vbfs->lock_inode);
	ret = vbfs_inode_sync_unlocked(i_vbfs);
	pthread_mutex_unlock(&i_vbfs->lock_inode);

	return ret;
}

static int inode_free(struct inode_vbfs *i_vbfs)
{
	struct extend_content *extend_data = NULL;
	struct extend_content *tmp = NULL;
	__u32 extend_size = 0;

	log_err("inode_free ENTER\n");
	extend_size = vbfs_ctx.super.s_extend_size;

	if (i_vbfs->inode_first_ext) {
		mp_free(i_vbfs->inode_first_ext, extend_size);
		i_vbfs->inode_first_ext = NULL;
	}

	list_for_each_entry_safe(extend_data, tmp, &i_vbfs->data_buf_list, data_list) {
		if (extend_data->extend_dirty != 0) {
			log_err("BUG\n");
		}

		if (extend_data->extend_buf) {
			mp_free(extend_data->extend_buf, extend_size);
			extend_data->extend_buf = NULL;
		}

		list_del(&extend_data->data_list);

		mp_free(extend_data, sizeof(struct extend_content));
	}

	mp_free(i_vbfs, sizeof(struct inode_vbfs));

	log_err("inode_free EXIT\n");

	return 0;
}

static int vbfs_inode_free(struct inode_vbfs *i_vbfs)
{
	int ret = 0;

	pthread_mutex_lock(&i_vbfs->lock_inode);
	vbfs_inode_sync_unlocked(i_vbfs);
	ret = inode_free(i_vbfs);
	pthread_mutex_unlock(&i_vbfs->lock_inode);

	return ret;
}

int vbfs_inode_close(struct inode_vbfs *i_vbfs)
{
	struct inode_vbfs *inode_v = NULL;
	struct inode_vbfs *tmp = NULL;

	log_dbg("vbfs_inode_close %u ENTER\n", i_vbfs->i_ino);

	pthread_mutex_lock(&vbfs_ctx.lock_active_inode);

	list_for_each_entry_safe(inode_v, tmp, &vbfs_ctx.active_inode_list, inode_l) {
		if (inode_v->i_ino == i_vbfs->i_ino) {
			if (--i_vbfs->nref == 0) {
				list_del(&i_vbfs->inode_l);
				vbfs_inode_free(i_vbfs);
			}
		}
	}

	pthread_mutex_unlock(&vbfs_ctx.lock_active_inode);

	log_dbg("vbfs_inode_close EXIT\n");

	return 0;
}

int vbfs_inode_mark_dirty(struct inode_vbfs *i_vbfs)
{
	i_vbfs->inode_dirty = 1;

	return 0;
}

int inode_get_first_extend_unlocked(struct inode_vbfs *inode_v)
{
	__u32 extend_size = 0;

	extend_size = vbfs_ctx.super.s_extend_size;

	if (inode_v->first_ext_status != 0) {
		return 0;
	}

	inode_v->inode_first_ext = mp_valloc(extend_size);
	if (inode_v->inode_first_ext == NULL)
		return -ENOMEM;

	if (read_extend(inode_v->i_extend, inode_v->inode_first_ext)) {
		mp_free(inode_v->inode_first_ext, extend_size);
		return -EIO;
	}

	log_dbg("inode_get_first_extend_unlocked, get extend %u\n", inode_v->i_extend);

	inode_v->first_ext_status = 1;

	return 0;
}

int vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode_parent, const char *name, __u32 *ino)
{
	struct list_head dir_list;
	int ret = 0;
	struct dentry_vbfs *dentry = NULL;
	int found = 0;

	INIT_LIST_HEAD(&dir_list);

	ret = get_dentry(v_inode_parent, &dir_list);
	if (ret)
		return ret;

	/* processing */
	list_for_each_entry(dentry, &dir_list, dentry_list) {
		if (0 == strncmp(dentry->name, name, NAME_LEN - 1)) {
			found = 1;
			break;
		}
	}

	put_dentry(&dir_list);

	if (found) {
		return 0;
	} else {
		return -ENOENT;
	}
}

struct inode_vbfs *vbfs_pathname_to_inode(const char *pathname, int *err_no)
{
	struct inode_vbfs *inode_v = NULL;
	__u32 ino = 0;
	char *name = NULL;
	char *pos = NULL;
	char *subname = NULL;

	log_dbg("vbfs_pathname_to_inode %s, ENTER\n", pathname);

	name = strdup(pathname);
	if (name == NULL) {
		*err_no = -ENOMEM;
		return NULL;
	}
	pos = name;

	inode_v = vbfs_inode_open(ROOT_INO, err_no);
	if (*err_no) {
		return NULL;
	}

	while ((subname = pathname_str_sep(&pos, PATH_SEP)) != NULL) {
		if (strlen(subname) == 0)
			continue;

		*err_no = vbfs_inode_lookup_by_name(inode_v, subname, &ino);
		vbfs_inode_close(inode_v);
		if (*err_no) {
			return NULL;
		}

		inode_v = vbfs_inode_open(ino, err_no);
		if (*err_no) {
			return NULL;
		}
	}

	free(name);

	log_dbg("vbfs_pathname_to_inode EXIT\n", pathname);

	return inode_v;
}
