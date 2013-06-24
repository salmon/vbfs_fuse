#include "log.h"
#include "err.h"
#include "vbfs-fuse.h"

void fill_stbuf_by_dirent(struct stat *stbuf, struct vbfs_dirent *dirent)
{
	stbuf->st_ino = dirent->i_ino;

	if (dirent->i_mode == VBFS_FT_DIR) {
		stbuf->st_mode = S_IFDIR | 0777;
	} else if (dirent->i_mode == VBFS_FT_REG_FILE) {
		stbuf->st_mode = S_IFREG | 0777;
	}

	stbuf->st_size = dirent->i_size;

	stbuf->st_atime = dirent->i_atime;
	stbuf->st_mtime = dirent->i_mtime;
	stbuf->st_ctime = dirent->i_ctime;
}

static void load_dirent_header(vbfs_dir_header_dk_t *dh_dk, struct vbfs_dirent_header *dh)
{
	dh->group_no = le32_to_cpu(dh_dk->vbfs_dir_header.group_no);
	dh->total_extends = le32_to_cpu(dh_dk->vbfs_dir_header.total_extends);
	dh->dir_self_count = le32_to_cpu(dh_dk->vbfs_dir_header.dir_self_count);
	dh->dir_total_count = le32_to_cpu(dh_dk->vbfs_dir_header.dir_total_count);
	dh->next_extend = le32_to_cpu(dh_dk->vbfs_dir_header.next_extend);
	dh->dir_capacity = le32_to_cpu(dh_dk->vbfs_dir_header.dir_capacity);
	dh->bitmap_size = le32_to_cpu(dh_dk->vbfs_dir_header.bitmap_size);
}

static void save_dirent_header(vbfs_dir_header_dk_t *dh_dk, struct vbfs_dirent_header *dh)
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

	dir->name[NAME_LEN] = '\0';
	strncpy(dir->name, dir_dk->name, NAME_LEN - 1);
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

	strncpy(dir_dk->name, dir->name, NAME_LEN - 1);
}

static void __link_active_inode(struct inode_info *inode)
{
	struct active_inode *active_i;
	uint32_t ino;

	active_i = get_active_inode();
	ino = inode->dirent->i_ino;

	list_add(&inode->active_list, &active_i->inode_list);
	hlist_add_head(&inode->hash_list, &active_i->inode_cache[INODE_HASH(ino)]);
}

static void __unlink_active_inode(struct inode_info *inode)
{
	hlist_del(&inode->hash_list);
	list_del(&inode->active_list);
}

static struct inode_info *__find_active_inode(const uint32_t ino)
{
	struct active_inode *active_i;
	struct inode_info *inode;

	active_i = get_active_inode();

	hlist_for_each_entry(inode, &active_i->inode_cache[INODE_HASH(ino)],
				hash_list) {
		if (ino == inode->dirent->i_ino)
			return inode;
	}

	return NULL;
}

static struct inode_info *__alloc_inode(struct vbfs_dirent *dir, int pos)
{
	struct inode_info *inode;
	struct vbfs_dirent *dir_tmp; 

	inode = mp_malloc(sizeof(*inode));
	if (NULL == inode) {
		return ERR_PTR(-ENOMEM);
	}
	dir_tmp = mp_malloc(sizeof(*dir_tmp));
	memcpy(dir_tmp, dir, sizeof(struct vbfs_dirent));
	inode->dirent = dir_tmp;
	inode->position = pos;
	inode->status = CLEAN;
	inode->flags = 0;
	inode->ref = 1;
	pthread_mutex_init(&inode->lock, NULL);
	INIT_LIST_HEAD(&inode->extend_list);

	return inode;
}

static struct inode_info *__get_inode_by_dirent(struct vbfs_dirent *dir, int pos)
{
	struct inode_info *inode;

	inode = __find_active_inode(dir->i_ino);
	if (NULL != inode) {
		inode->ref ++;
		return inode;
	}

	inode = __alloc_inode(dir, pos);
	if (IS_ERR(inode))
		return inode;

	__link_active_inode(inode);

	return inode;
}

int init_root_inode(void)
{
	int ret = 0, pos = 0;
	char *data, *buf;
	struct inode_info *inode;
	struct vbfs_dirent root_dir; 
	struct vbfs_dirent_header root_header;
	struct extend_buf *b;

	buf = extend_read(get_data_queue(), ROOT_INO, &b);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		return ret;
	}

	data = buf;
	load_dirent_header((vbfs_dir_header_dk_t *) data, &root_header);
	init_dir_bm_size(root_header.bitmap_size);
	init_dir_capacity(root_header.dir_capacity);

	data = buf + VBFS_DIR_META_SIZE +
			VBFS_DIR_SIZE * (get_dir_bm_size() + pos);
	load_dirent((struct vbfs_dirent_disk *) data, &root_dir);

	extend_put(b);

	inode = __alloc_inode(&root_dir, pos);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		return ret;
	}
	__link_active_inode(inode);

	return 0;
}

int vbfs_update_times(struct inode_info *inode, time_update_flags mask)
{
	pthread_mutex_lock(&inode->lock);

	if (mask & UPDATE_ATIME)
		inode->dirent->i_atime = time(NULL);
	if (mask & UPDATE_MTIME)
		inode->dirent->i_mtime = time(NULL);
	if (mask & UPDATE_CTIME)
		inode->dirent->i_ctime = time(NULL);

	inode->status = DIRTY;

	pthread_mutex_unlock(&inode->lock);

	return 0;
}

static struct inode_info *__open_inode(char *buf, const char *subname, int is_root)
{
	int pos;
	struct vbfs_bitmap bm;
	struct vbfs_dirent dir;
	struct inode_info *inode;
	char *data;

	bm.map_len = get_dir_bm_size() * VBFS_DIR_SIZE;
	bm.max_bit = get_dir_capacity();
	bm.bitmap = (uint32_t *)(buf + VBFS_DIR_META_SIZE);

	if (is_root)
		pos = 1;
	else
		pos = 0;

	while (1) {
		pos = bitmap_next_set_bit(&bm, pos);
		if (pos < 0)
			break;
		data = buf + VBFS_DIR_META_SIZE +
				VBFS_DIR_SIZE * (get_dir_bm_size() + pos);
		load_dirent((struct vbfs_dirent_disk *) data, &dir);

		if (strncmp(dir.name, subname, NAME_LEN - 1) == 0) {
			inode = __get_inode_by_dirent(&dir, pos);
			return inode;
		}
	}

	return ERR_PTR(-ENOENT);
}

static void active_inode_lock()
{
	struct active_inode *active_i;

	active_i = get_active_inode();
	pthread_mutex_lock(&active_i->lock);
}

static void active_inode_unlock()
{
	struct active_inode *active_i;

	active_i = get_active_inode();
	pthread_mutex_unlock(&active_i->lock);
}

static struct inode_info *get_root_inode()
{
	struct inode_info *inode;

	active_inode_lock();
	inode = __find_active_inode(ROOT_INO);
	active_inode_unlock();

	return inode;
}

static void free_inode(struct inode_info *inode)
{
	__unlink_active_inode(inode);
	pthread_mutex_destroy(&inode->lock);
	mp_free(inode->dirent);
	mp_free(inode);
}

static int writeback_inode(struct inode_info *inode)
{
	int ret = 0;
	struct extend_buf *b;
	char *buf, *data;
	uint32_t data_no;

	if (CLEAN == inode->status)
		return 0;

	data_no = inode->dirent->i_pino;
	buf = extend_read(get_data_queue(), data_no, &b);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		return ret;
	}

	data = buf + VBFS_DIR_META_SIZE +
		VBFS_DIR_SIZE * (get_dir_bm_size() + inode->position);
	save_dirent((struct vbfs_dirent_disk *) data, inode->dirent);

	extend_mark_dirty(b);
	ret = extend_write_dirty(b);
	extend_put(b);

	return ret;
}

int vbfs_inode_sync(struct inode_info *inode)
{
	struct extend_buf *b;

	pthread_mutex_lock(&inode->lock);
	writeback_inode(inode);

	list_for_each_entry(b, &inode->extend_list, inode_list) {
		extend_write_dirty(b);
	}
	pthread_mutex_unlock(&inode->lock);

	return 0;
}

int vbfs_inode_close(struct inode_info *inode)
{
	vbfs_inode_sync(inode);

	if (ROOT_INO == inode->dirent->i_ino)
		return 0;

	active_inode_lock();
	if (0 == --inode->ref) {
		free_inode(inode);
	}
	inode = __find_active_inode(inode->dirent->i_pino);
	if (NULL == inode) {
		log_err("BUG");
		return -1;
	}
	active_inode_unlock();

	if (vbfs_inode_close(inode) < 0)
		return -1;

	return 0;
}

struct inode_info *inode_lookup_by_name(struct inode_info *inode_parent, const char *subname)
{
	int ret = 0, need_put = 0, is_root;
	uint32_t data_no;
	char *data;
	struct extend_buf *ebuf;
	struct inode_info *inode;
	struct vbfs_dirent_header dir_header;

	data_no = inode_parent->dirent->i_ino;

	while (1) {
		data = extend_read(get_data_queue(), data_no, &ebuf);
		if (IS_ERR(data)) {
			ret = PTR_ERR(data);
			goto err;
		}
		need_put = 1;

		load_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);

		if (ROOT_INO == data_no)
			is_root = 1;
		else
			is_root = 0;

		inode = __open_inode(data, subname, is_root);
		if (IS_ERR(inode)) {
			ret = PTR_ERR(inode);
			if (ret != -ENOENT)
				goto err;
		}

		extend_put(ebuf);
		need_put = 0;

		if (dir_header.dir_self_count) {
			if (dir_header.next_extend != 0)
				data_no = dir_header.next_extend;
			else
				return ERR_PTR(-ENOENT);
		}
	}

	return 0;

err:
	if (need_put)
		extend_put(ebuf);

	return ERR_PTR(ret);
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
			vbfs_inode_close(inode);
			free(name);
			return ERR_PTR(ret);
		}
		inode = inode_tmp;
		inode_tmp = NULL;

		//vbfs_inode_close(inode);
	}

	free(name);

	return inode;
}

int vbfs_readdir(struct inode_info *inode, off_t filler_pos,
		fuse_fill_dir_t filler, void *filler_buf)
{
	/* Emulate . and .. directory. */
	return 0;
}

