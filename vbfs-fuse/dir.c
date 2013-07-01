#include "log.h"
#include "err.h"
#include "vbfs-fuse.h"

void fill_stbuf_by_dirent(struct stat *stbuf, struct vbfs_dirent *dirent)
{
	memset(stbuf, 0, sizeof(struct stat));

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

static void load_dirent(struct vbfs_dirent_disk *dir_dk, struct vbfs_dirent *dir)
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

static void save_dirent(struct vbfs_dirent_disk *dir_dk, struct vbfs_dirent *dir)
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
		if (ino == inode->dirent->i_ino) {
			if (! inode->flags & INODE_REMOVE)
				return inode;
		}
	}

	return NULL;
}

static struct inode_info *find_active_inode(const uint32_t ino)
{
	struct inode_info *inode;

	active_inode_lock();
	inode = __find_active_inode(ino);
	active_inode_unlock();

	return inode;
}

static struct inode_info *__alloc_inode(struct vbfs_dirent *dir,
					int pos, const uint32_t data_no)
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
	inode->data_no = data_no;
	inode->position = pos;
	inode->status = CLEAN;
	inode->flags = 0;
	inode->ref = 1;
	pthread_mutex_init(&inode->lock, NULL);
	INIT_LIST_HEAD(&inode->extend_list);

	return inode;
}

static struct inode_info *__get_inode_by_dirent(struct vbfs_dirent *dir,
					int pos, const uint32_t data_no)
{
	struct inode_info *inode;

	inode = __find_active_inode(dir->i_ino);
	if (NULL != inode) {
		inode->ref ++;
		return inode;
	}

	inode = __alloc_inode(dir, pos, data_no);
	if (IS_ERR(inode))
		return inode;

	__link_active_inode(inode);

	return inode;
}

static struct inode_info *get_inode_by_dirent(struct vbfs_dirent *dir,
					int pos, const uint32_t data_no)
{
	struct inode_info *inode;

	active_inode_lock();
	inode = __get_inode_by_dirent(dir, pos, data_no);
	active_inode_unlock();

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
	log_err("%u %u\n", root_header.bitmap_size, root_header.dir_capacity);

	data = buf + VBFS_DIR_META_SIZE +
			VBFS_DIR_SIZE * (get_dir_bm_size() + pos);
	load_dirent((struct vbfs_dirent_disk *) data, &root_dir);

	extend_put(b);

	inode = __alloc_inode(&root_dir, pos, ROOT_INO);
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

static struct inode_info *__open_inode(char *buf, const uint32_t data_no,
				const char *subname, int is_root)
{
	int pos;
	struct vbfs_bitmap bm;
	struct vbfs_dirent dir;
	struct inode_info *inode;
	char *data;

	init_bitmap(&bm, get_dir_capacity());
	bm.bitmap = (uint32_t *)(buf + VBFS_DIR_META_SIZE);

	if (is_root)
		pos = 0;
	else
		pos = -1;

	while (1) {
		pos = bitmap_next_set_bit(&bm, pos);
		if (pos < 0)
			break;
		data = buf + VBFS_DIR_META_SIZE +
				VBFS_DIR_SIZE * (get_dir_bm_size() + pos);
		load_dirent((struct vbfs_dirent_disk *) data, &dir);

		//log_dbg("dir.name %s, subname %s, pos %d", dir.name, subname, pos);
		if (strncmp(dir.name, subname, NAME_LEN - 1) == 0) {
			inode = get_inode_by_dirent(&dir, pos, data_no);
			return inode;
		}
	}

	return ERR_PTR(-ENOENT);
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

static int __writeback_inode(struct inode_info *inode)
{
	int ret = 0;
	struct extend_buf *b;
	char *buf, *data;
	uint32_t data_no;

	if (CLEAN == inode->status)
		return 0;

	data_no = inode->data_no;
	//log_dbg("%u %u", data_no, inode->position);
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
	/* if ret == 0 */
	inode->status = CLEAN;

	return ret;
}

int vbfs_inode_sync(struct inode_info *inode)
{
	struct extend_buf *b;

	pthread_mutex_lock(&inode->lock);
	__writeback_inode(inode);

	list_for_each_entry(b, &inode->extend_list, inode_list) {
		extend_write_dirty(b);
	}
	pthread_mutex_unlock(&inode->lock);

	return 0;
}

int vbfs_inode_close(struct inode_info *inode)
{
	uint32_t pino;

	if (! inode->flags & INODE_REMOVE)
		vbfs_inode_sync(inode);

	if (ROOT_INO == inode->dirent->i_ino)
		return 0;

	active_inode_lock();
	pino = inode->dirent->i_pino;
	if (0 == --inode->ref)
		free_inode(inode);
	inode = __find_active_inode(pino);
	if (NULL == inode) {
		log_err("BUG");
		active_inode_unlock();
		return -1;
	}
	active_inode_unlock();

	if (vbfs_inode_close(inode) < 0)
		return -1;

	return 0;
}

static int __readdir_by_inode(struct inode_info *inode, off_t filler_pos,
				fuse_fill_dir_t filler, void *filler_buf)
{
	int pos;
	uint32_t data_no;
	char *data, *data_pos;
	struct vbfs_bitmap bm;
	struct stat stbuf;
	struct extend_buf *ebuf;
	struct vbfs_dirent dir;
	struct vbfs_dirent_header dir_header;

	data_no = inode->dirent->i_ino;

	while (1) {
		data = extend_read(get_data_queue(), data_no, &ebuf);
		if (IS_ERR(data))
			return PTR_ERR(data);

		load_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);
		if (dir_header.dir_self_count) {
			if (ROOT_INO == data_no)
				pos = 0;
			else
				pos = -1;

			/* readdir */
			init_bitmap(&bm, get_dir_capacity());
			bm.bitmap = (uint32_t *)(data + VBFS_DIR_META_SIZE);
			int j = 0;

			while (1) {
				pos = bitmap_next_set_bit(&bm, pos);
				if (pos < 0)
					break;
				data_pos = data + VBFS_DIR_META_SIZE +
						VBFS_DIR_SIZE * (get_dir_bm_size() + pos);
				load_dirent((struct vbfs_dirent_disk *) data_pos, &dir);

				fill_stbuf_by_dirent(&stbuf, &dir);
				filler(filler_buf, dir.name, &stbuf, 0);
				j ++;
				if (j >= dir_header.dir_self_count)
					break;
			}
		}

		extend_put(ebuf);

		if (dir_header.next_extend != 0)
			data_no = dir_header.next_extend;
		else
			return 0;
	}
}

static struct inode_info *__inode_lookup_by_name(struct inode_info *inode_parent,
					const char *subname)
{
	int ret = 0, need_put = 0, is_root;
	uint32_t data_no;
	char *data;
	struct extend_buf *ebuf;
	struct inode_info *inode;
	struct vbfs_dirent_header dir_header;

	if (strncmp(subname, ".", NAME_LEN - 1) == 0)
		return inode_parent;

	if (strncmp(subname, "..", NAME_LEN - 1) == 0) {
		inode = find_active_inode(inode_parent->dirent->i_pino);
		if (NULL == inode) {
			log_err("BUG");
			return ERR_PTR(-1);
		} else
			return inode;
	}

	data_no = inode_parent->dirent->i_ino;

	while (1) {
		data = extend_read(get_data_queue(), data_no, &ebuf);
		if (IS_ERR(data)) {
			ret = PTR_ERR(data);
			goto err;
		}
		need_put = 1;

		load_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);
		if (dir_header.dir_self_count) {
			if (ROOT_INO == data_no)
				is_root = 1;
			else
				is_root = 0;

			/* lookup */
			inode = __open_inode(data, data_no, subname, is_root);
			if (IS_ERR(inode)) {
				ret = PTR_ERR(inode);
				if (ret != -ENOENT)
					goto err;
			} else {
				extend_put(ebuf);
				return inode;
			}
		}


		extend_put(ebuf);
		need_put = 0;

		if (dir_header.next_extend != 0)
			data_no = dir_header.next_extend;
		else
			return ERR_PTR(-ENOENT);
	}

	return ERR_PTR(-ENOENT);

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

		pthread_mutex_lock(&inode->lock);
		inode_tmp = __inode_lookup_by_name(inode, subname);
		pthread_mutex_unlock(&inode->lock);
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

int __vbfs_readdir(struct inode_info *inode, off_t filler_pos,
		fuse_fill_dir_t filler, void *filler_buf)
{
	struct inode_info *inode_tmp;
	struct stat stbuf;
	int ret = 0;

	/* Emulate . and .. directory. */
	fill_stbuf_by_dirent(&stbuf, inode->dirent);
	filler(filler_buf, ".", &stbuf, 0);

	inode_tmp = find_active_inode(inode->dirent->i_pino);
	if (inode_tmp) {
		fill_stbuf_by_dirent(&stbuf, inode_tmp->dirent);
		filler(filler_buf, "..", &stbuf, 0);
	}

	/* Get other dirs */
	ret = __readdir_by_inode(inode, filler_pos, filler, filler_buf);

	return ret;
}

int vbfs_readdir(struct inode_info *inode, off_t filler_pos,
		fuse_fill_dir_t filler, void *filler_buf)
{
	int ret;

	pthread_mutex_lock(&inode->lock);
	ret = __vbfs_readdir(inode, filler_pos, filler, filler_buf);
	pthread_mutex_unlock(&inode->lock);

	return ret;
}

static void init_dirent(struct vbfs_dirent *dir, uint32_t ino, uint32_t pino, uint32_t mode)
{
	dir->i_ino = ino;
	dir->i_pino = pino;
	dir->i_mode = mode;

	if (VBFS_FT_DIR == mode)
		dir->i_size = get_extend_size();
	else 
		dir->i_size = 0;

	dir->i_atime = time(NULL);
	dir->i_mtime = time(NULL);
	dir->i_ctime = time(NULL);
}

static void init_dirent_header(struct vbfs_dirent_header *dir_header, uint32_t group_no)
{
	dir_header->bitmap_size = get_dir_bm_size();
	dir_header->dir_capacity = get_dir_capacity();
	dir_header->group_no = group_no;
	dir_header->next_extend = 0;
	dir_header->dir_self_count = 1;
	dir_header->total_extends = 1; /* useless now */
	dir_header->dir_total_count = 1; /* useless now */
}

static void new_dirent_header(struct vbfs_dirent_header *dir_header)
{
	dir_header->bitmap_size = get_dir_bm_size();
	dir_header->dir_capacity = get_dir_capacity();
	dir_header->group_no = 0;
	dir_header->next_extend = 0;
	dir_header->dir_self_count = 0;
	dir_header->total_extends = 0; /* useless now */
	dir_header->dir_total_count = 0; /* useless now */
}

static int init_newdir_extend(uint32_t eno)
{
	char *data;
	struct extend_buf *b;
	struct vbfs_dirent_header dir_header;

	data = extend_new(get_data_queue(), eno, &b);
	if (IS_ERR(data))
		return PTR_ERR(data);

	memset(data, 0, get_extend_size());
	new_dirent_header(&dir_header);
	save_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);

	extend_mark_dirty(b);
	extend_write_dirty(b);
	extend_put(b);

	return 0;
}

static int __vbfs_parent_fill_dir(uint32_t data_no, uint32_t pino, char *subname,
			struct vbfs_dirent_header *dir_header, int is_new, uint32_t mode)
{
	int pos, ret = 0;
	char *data, *data_pos;
	struct vbfs_bitmap bm;
	struct extend_buf *b;
	struct vbfs_dirent dir;
	uint32_t ino;

	if (is_new) {
		init_dirent_header(dir_header, dir_header->group_no + 1);
		data = extend_new(get_data_queue(), data_no, &b);
	} else {
		dir_header->dir_self_count ++;
		data = extend_read(get_data_queue(), data_no, &b);
	}
	if (IS_ERR(data))
		return PTR_ERR(data);
	if (is_new)
		memset(data, 0, get_extend_size());

	init_bitmap(&bm, get_dir_capacity());
	bm.bitmap = (uint32_t *)(data + VBFS_DIR_META_SIZE);

	pos = bitmap_next_clear_bit(&bm, -1);
	if (pos < 0) {
		log_err("BUG");
		return -EINVAL;
	}

	ret = alloc_extend_bitmap(&ino);
	if (ret) {
		extend_put(b);
		return ret;
	}

	if (VBFS_FT_DIR == mode)
		init_newdir_extend(ino);

	save_dirent_header((vbfs_dir_header_dk_t *) data, dir_header);
	data_pos = data + VBFS_DIR_META_SIZE +
			VBFS_DIR_SIZE * (get_dir_bm_size() + pos);

	init_dirent(&dir, ino, pino, mode);
	strncpy(dir.name, subname, NAME_LEN - 1);
	save_dirent((struct vbfs_dirent_disk *) data_pos, &dir);
	log_dbg("%u, new inode no is %u, pos is %u", data_no, ino, pos);
	bitmap_set_bit(&bm, pos);

	extend_mark_dirty(b);
	extend_write_dirty(b);
	extend_put(b);

	return 0;
}

static int __vbfs_create(struct inode_info *inode, char *subname, uint32_t mode)
{
	int pos, has_room = 0, ret = 0;
	uint32_t data_no, data_no_mk;
	char *data, *data_pos;
	struct vbfs_bitmap bm;
	struct extend_buf *ebuf;
	struct vbfs_dirent dir;
	struct vbfs_dirent_header dir_header, dir_header_tmp;

	data_no = inode->dirent->i_ino;
	if (inode->dirent->i_mode != VBFS_FT_DIR)
		return -ENOTDIR;

	while (1) {
		data = extend_read(get_data_queue(), data_no, &ebuf);
		if (IS_ERR(data))
			return PTR_ERR(data);

		load_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);

		if (ROOT_INO == data_no)
			pos = 0;
		else
			pos = -1;

		if (dir_header.dir_self_count < get_dir_capacity()) {
			has_room = 1;
			data_no_mk = data_no;
		}

		/* checkdir */
		init_bitmap(&bm, get_dir_capacity());
		bm.bitmap = (uint32_t *)(data + VBFS_DIR_META_SIZE);

		while (1) {
			pos = bitmap_next_set_bit(&bm, pos);
			if (pos < 0)
				break;
			data_pos = data + VBFS_DIR_META_SIZE +
					VBFS_DIR_SIZE * (get_dir_bm_size() + pos);
			load_dirent((struct vbfs_dirent_disk *) data_pos, &dir);
			if (strncmp(dir.name, subname, NAME_LEN - 1) == 0) {
				extend_put(ebuf);
				return -EEXIST;
			}
		}

		if (dir_header.next_extend != 0) {
			extend_put(ebuf);
			data_no = dir_header.next_extend;
		} else
			break;
	}

	memcpy(&dir_header_tmp, &dir_header, sizeof(dir_header));
	if (! has_room) {
		uint32_t new_data_no;

		ret = alloc_extend_bitmap(&new_data_no);
		if (ret) {
			extend_put(ebuf);
			return ret;
		}

		ret = __vbfs_parent_fill_dir(new_data_no, inode->dirent->i_ino,
					subname, &dir_header_tmp, 1, mode);
		goto err;

		dir_header.next_extend = new_data_no;
		data_pos = ebuf->data;
		save_dirent_header((vbfs_dir_header_dk_t *) data_pos, &dir_header);
		extend_mark_dirty(ebuf);
		extend_write_dirty(ebuf);
	} else
		ret = __vbfs_parent_fill_dir(data_no_mk, inode->dirent->i_ino,
						subname, &dir_header_tmp, 0, mode);


err:
	extend_put(ebuf);

	return ret;
}

int vbfs_create(struct inode_info *inode, char *subname, uint32_t mode)
{
	int ret;

	pthread_mutex_lock(&inode->lock);
	ret = __vbfs_create(inode, subname, mode);
	pthread_mutex_unlock(&inode->lock);

	return ret;
}

int __vbfs_truncate(struct inode_info *inode, off_t size)
{
	struct extend_buf *b;
	char *data;
	uint32_t offset, data_no, fst_data_len;
	uint32_t *start_off, *p_idx;
	int i, j;
	uint64_t tmp;

	if (inode->dirent->i_size <= size)
		return 0;

	fst_data_len = get_extend_size() - get_file_idx_size();

	log_dbg("size %llu, fst %u", inode->dirent->i_size, fst_data_len);
	if (inode->dirent->i_size <= fst_data_len)
		return 0;

	data = extend_read(get_data_queue(), inode->dirent->i_ino, &b);
	if (IS_ERR(data))
		return PTR_ERR(data);

	/*   */
	if (size > fst_data_len) {
		tmp = inode->dirent->i_size - size;
		if (tmp % get_extend_size())
			i = tmp / get_extend_size() + 1;
		else
			i = tmp / get_extend_size();
	} else
		i = 0;

	start_off = (uint32_t *) data;
	start_off += i;

	tmp = inode->dirent->i_size - fst_data_len;
	if (tmp % get_extend_size())
		j = tmp / get_extend_size() + 1;
	else
		j = tmp / get_extend_size();
	offset = j - i;
	log_dbg("size %llu, offset %u", inode->dirent->i_size, offset);
	for (i = 0; i < offset; i ++) {
		p_idx = start_off + i;
		data_no = le32_to_cpu(*p_idx);
		log_dbg("data_no %u", data_no);
		free_extend_bitmap_async(data_no);
	}

	queue_write_dirty(get_meta_queue());
	extend_put(b);

	inode->dirent->i_size = size;
	inode->status = DIRTY;

	return 0;
}

int vbfs_truncate(struct inode_info *inode, off_t size)
{
	int ret;

	pthread_mutex_lock(&inode->lock);
	ret = __vbfs_truncate(inode, size);
	pthread_mutex_unlock(&inode->lock);

	vbfs_inode_sync(inode);

	return ret;
}

static int __vbfs_check_empty(struct inode_info *inode)
{
	struct extend_buf *b;
	char *data;
	struct vbfs_dirent_header dir_header;
	uint32_t data_no;

	data_no = inode->dirent->i_ino;

	while (1) {
		data = extend_read(get_data_queue(), data_no, &b);
		if (IS_ERR(data))
			return PTR_ERR(data);

		load_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);
		extend_put(b);
		if (dir_header.dir_self_count)
			return -1;
		if (dir_header.next_extend == 0)
			break;
		else
			data_no = dir_header.next_extend;
	}

	return 0;
}

static int __vbfs_remove_inode(struct inode_info *inode)
{
	int ret = 0;
	struct extend_buf *b;
	char *data;
	uint32_t data_no;
	struct vbfs_dirent_header dir_header;
	struct vbfs_bitmap bm;

	data_no = inode->data_no;
	data = extend_read(get_data_queue(), data_no, &b);
	if (IS_ERR(data)) {
		ret = PTR_ERR(data);
		return ret;
	}

	load_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);
	if (dir_header.dir_self_count --) {
		/* a lot of work todo */
	}

	init_bitmap(&bm, get_dir_capacity());
	bm.bitmap = (uint32_t *)(data + VBFS_DIR_META_SIZE);
	log_dbg("%u data_no %u pos %u", inode->dirent->i_ino, data_no, inode->position);
	bitmap_clear_bit(&bm, inode->position);

	save_dirent_header((vbfs_dir_header_dk_t *) data, &dir_header);

	extend_mark_dirty(b);
	extend_write_dirty(b);
	extend_put(b);

	return 0;
}

static int __vbfs_remove(struct inode_info *inode)
{
	//struct inode_info *parent;

	free_extend_bitmap(inode->dirent->i_pino);
	/* Fix */
	//__unlink_active_inode(inode);
	inode->flags |= INODE_REMOVE;

	//parent = __find_active_inode(inode->dirent->i_pino);

	pthread_mutex_lock(&inode->lock);
	__vbfs_remove_inode(inode);
	pthread_mutex_unlock(&inode->lock);

	return 0;
}

static int __vbfs_rmdir(struct inode_info *inode)
{
	if (inode->dirent->i_ino == ROOT_INO) {
		//log_dbg("%d, %s", inode->ref, inode->dirent->name);
		return -EBUSY;
	}

	/* check is empty */
	if (__vbfs_check_empty(inode))
		return -ENOTEMPTY;

	/* remove inode */
	__vbfs_remove(inode);

	return 0;
}

int vbfs_rmdir(struct inode_info *inode)
{
	int ret;

	if (inode->dirent->i_mode != VBFS_FT_DIR)
		return -ENOTDIR;

	active_inode_lock();
	ret = __vbfs_rmdir(inode);
	active_inode_unlock();

	return ret;
}

int __vbfs_unlink(struct inode_info *inode)
{
	int ret;

	if (inode->ref > 1)
		return -EBUSY;

	/* truncate file */
	ret = vbfs_truncate(inode, 0);
	if (ret)
		return ret;

	/* remove inode */
	__vbfs_remove(inode);

	return 0;
}

int vbfs_unlink(struct inode_info *inode)
{
	int ret;

	if (inode->dirent->i_mode == VBFS_FT_DIR)
		return -EISDIR;

	active_inode_lock();
	ret = __vbfs_unlink(inode);
	active_inode_unlock();

	return ret;
}
