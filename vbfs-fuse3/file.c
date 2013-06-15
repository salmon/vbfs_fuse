#include "mempool.h"
#include "log.h"
#include "vbfs-fuse.h"

extern vbfs_fuse_context_t vbfs_ctx;

struct extend_data *alloc_edata_by_inode_unlocked(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no)
{
	struct extend_data *edata = NULL;

/*
	list_for_each_entry(edata, &inode_v->data_buf_list, data_list) {
		if (extend_no == edata->extend_no) {
			pthread_mutex_lock(&edata->ed_lock);
			edata->inode_ref ++;
			pthread_mutex_unlock(&edata->ed_lock);

			return edata;
		}
	}
*/

	edata = open_edata(extend_no, &vbfs_ctx.data_queue, err_no);
	if (*err_no) {
		return NULL;
	}

	if (edata->status == BUFFER_NOT_READY)
		edata->buf = mp_valloc(get_extend_size());
	else
		log_err("BUG");

	if (NULL == edata->buf) {
		*err_no = -ENOMEM;
		close_edata(edata);
		return NULL;
	}
	memset(edata->buf, 0, sizeof(get_extend_size()));
	edata->status = BUFFER_DIRTY;
	edata->inode_ref = 1;

	list_add_tail(&edata->data_list, &inode_v->data_buf_list);

	return edata;
}

struct extend_data *alloc_edata_by_inode(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no)
{
	struct extend_data *edata = NULL;

	pthread_mutex_lock(&inode_v->inode_lock);
	edata = alloc_edata_by_inode_unlocked(extend_no, inode_v, err_no);
	pthread_mutex_unlock(&inode_v->inode_lock);

	return edata;
}

struct extend_data *get_edata_by_inode_unlocked(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no)
{
	struct extend_data *edata = NULL;

	list_for_each_entry(edata, &inode_v->data_buf_list, data_list) {
		if (extend_no == edata->extend_no) {
			pthread_mutex_lock(&edata->ed_lock);
			edata->inode_ref ++;
			pthread_mutex_unlock(&edata->ed_lock);

			return edata;
		}
	}

	edata = open_edata(extend_no, &vbfs_ctx.data_queue, err_no);
	if (*err_no) {
		return NULL;
	}

	read_edata(edata);
	if (BUFFER_NOT_READY == edata->status) {
		*err_no = -EIO;
		close_edata(edata);
		return NULL;
	}

	edata->inode_ref = 1;

	list_add_tail(&edata->data_list, &inode_v->data_buf_list);

	return edata;
}

struct extend_data *get_edata_by_inode(const __u32 extend_no,
				struct inode_vbfs *inode_v, int *err_no)
{
	struct extend_data *edata = NULL;

	pthread_mutex_lock(&inode_v->inode_lock);
	edata = get_edata_by_inode_unlocked(extend_no, inode_v, err_no);
	pthread_mutex_unlock(&inode_v->inode_lock);

	return edata;
}

int put_edata_by_inode_unlocked(const __u32 extend_no, struct inode_vbfs *inode_v)
{
	struct extend_data *edata = NULL;
	int need_free = 0;
	int ret = 0;

	list_for_each_entry(edata, &inode_v->data_buf_list, data_list) {
		if (extend_no == edata->extend_no) {
			pthread_mutex_lock(&edata->ed_lock);
			if (--edata->inode_ref == 0) {
				need_free = 1;
				list_del(&edata->data_list);
			}
			pthread_mutex_unlock(&edata->ed_lock);
			break;
		}
	}

	if (need_free) {
		ret = close_edata(edata);
	}

	return ret;
}

int put_edata_by_inode(const __u32 extend_no, struct inode_vbfs *inode_v)
{
	int ret = 0;

	pthread_mutex_lock(&inode_v->inode_lock);
	ret = put_edata_by_inode_unlocked(extend_no, inode_v);
	pthread_mutex_unlock(&inode_v->inode_lock);

	return ret;
}

/*
 *
 * */

int file_init_default_fst(struct inode_vbfs *inode_v)
{
	struct extend_data *edata = NULL;
	int ret = 0;

	edata = alloc_edata_by_inode(inode_v->i_extend, inode_v, &ret);
	if (ret)
		return ret;

	if (BUFFER_NOT_READY == edata->status) {
		log_err("BUG");
		return -1;
	}

	put_edata_by_inode(inode_v->i_extend, inode_v);

	return 0;
}

int vbfs_create_file(struct inode_vbfs *v_inode_parent, const char *name)
{
	struct inode_vbfs *inode_v = NULL;
	int ret = 0;

	inode_v = alloc_inode(v_inode_parent->i_ino, VBFS_FT_REG_FILE, &ret);
	if (ret) {
		return ret;
	}

	ret = add_dirent(v_inode_parent, inode_v, name);
	if (ret) {
		vbfs_inode_close(inode_v);
		return ret;
	}

	vbfs_inode_close(inode_v);

	return ret;
}

int sync_file(struct inode_vbfs *inode_v)
{
	return 0;
}

/* may optimize by get_first_entry */
struct extend_data *get_edata_by_file_idx(struct inode_vbfs *inode_v, int index, int *err_no)
{
	int ret = 0;
	struct extend_data *edata = NULL, *new_edata = NULL;
	__u32 *p_index = NULL;
	__u32 extend_no = 0;

	edata = get_edata_by_inode(inode_v->i_extend, inode_v, &ret);
	if (ret) {
		*err_no = ret;
		return NULL;
	}

	p_index = (__u32 *) edata->buf;
	p_index += index;
	extend_no = le32_to_cpu(*p_index);
	if (extend_no == 0) {
		ret = alloc_extend_bitmap(&extend_no);
		if (ret) {
			put_edata_by_inode(inode_v->i_extend, inode_v);
			*err_no = ret;
			return NULL;
		}

		*p_index = cpu_to_le32(extend_no);
		edata->status = BUFFER_DIRTY;

		new_edata = alloc_edata_by_inode(extend_no, inode_v, &ret);
		if (ret) {
			put_edata_by_inode(inode_v->i_extend, inode_v);
			*err_no = ret;
			return NULL;
		}
	} else {
		new_edata = get_edata_by_inode(extend_no, inode_v, &ret);
		if (ret) {
			put_edata_by_inode(inode_v->i_extend, inode_v);
			*err_no = ret;
			return NULL;
		}
	}

	put_edata_by_inode(inode_v->i_extend, inode_v);

	return new_edata;
}

int vbfs_read_buf(struct inode_vbfs *inode_v, char *buf, size_t size, off_t offset)
{
	int index = -1, ret = 0;
	size_t rd_len = 0;
	int buf_size = 0;
	off_t buf_off, tocopy;
	struct extend_data *edata = NULL;
	char *pos = NULL, *buf_pos = NULL;
	__u32 extend_no = 0;

	if (inode_v->i_size < offset) {
		return -EOVERFLOW;
	}

	buf_off = offset + get_file_idx_size();
	buf_size = (size + offset < inode_v->i_size) ? size : inode_v->i_size - offset;

	while (buf_size > 0) {
		if (buf_off < get_extend_size()) {
			extend_no = inode_v->i_extend;
			edata = get_edata_by_inode(inode_v->i_extend, inode_v, &ret);
			if (ret) {
				log_err("read error");
				return ret;
			}
		} else {
			index = buf_off / get_extend_size() - 1;
			edata = get_edata_by_file_idx(inode_v, index, &ret);
			extend_no = edata->extend_no;
			if (ret)
				break;
		}

		if (buf_size < get_extend_size() - buf_off % get_extend_size()) {
			tocopy = buf_size;
		} else {
			tocopy = get_extend_size() - buf_off % get_extend_size();
		}

		pos = edata->buf + buf_off % get_extend_size();
		buf_pos = buf + rd_len;
		memcpy(buf_pos, pos, tocopy);

		put_edata_by_inode(extend_no, inode_v);

		buf_off += tocopy;
		buf_size -= tocopy;
		rd_len += tocopy;
	}

	return rd_len;
}

int vbfs_write_buf(struct inode_vbfs *inode_v, const char *buf, size_t size, off_t offset)
{
	int index = -1, ret = 0, fill = 0;
	__u32 extend_no = 0;
	off_t len = 0;
	int buf_size;
	off_t buf_off, tocopy;
	char *pos = NULL;
	const char *src_pos = NULL;
	size_t wt_len = 0;
	struct extend_data *edata = NULL;

	len = size + offset + get_extend_size() - get_file_idx_size();
	if (len > (__u64) get_file_max_index() * get_extend_size())
		return -EFBIG;

	buf_off = offset + get_file_idx_size();
	buf_size = size;

	while (buf_size > 0) {
		if (buf_off < get_extend_size()) {
			extend_no = inode_v->i_extend;
			edata = get_edata_by_inode(inode_v->i_extend, inode_v, &ret);
			if (ret)
				return ret;
		} else {
			index = buf_off / get_extend_size() - 1;
			edata = get_edata_by_file_idx(inode_v, index, &ret);
			extend_no = edata->extend_no;
			if (ret)
				break;

		}

		if (buf_size < get_extend_size() - buf_off % get_extend_size()) {
			tocopy = buf_size;
			fill = 0;
		} else {
			tocopy = get_extend_size() - buf_off % get_extend_size();
			fill = 1;
		}

		pos = edata->buf + buf_off % get_extend_size();
		src_pos = buf + wt_len;
		memcpy(pos, src_pos, tocopy);

		edata->status = BUFFER_DIRTY;

		if (fill)
			put_edata_by_inode(extend_no, inode_v);

		buf_off += tocopy;
		buf_size -= tocopy;
		wt_len += tocopy;
	}

	if (inode_v->i_size < wt_len + offset) {
		log_err("i_size %u, write size %u", inode_v->i_size, wt_len + offset);
		inode_v->i_size = wt_len + offset;
		inode_v->inode_dirty = INODE_DIRTY;
	}

	return wt_len;
}

