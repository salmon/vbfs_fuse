#include "err.h"
#include "log.h"
#include "vbfs-fuse.h"

int sync_file(struct inode_info *inode)
{
	vbfs_inode_sync(inode);

	return 0;
}

static int __rd_ebuf_by_file_idx(struct inode_info *inode, int idx, struct extend_buf **bp)
{
	char *data;
	struct extend_buf *b;
	uint32_t *p_index, data_no;

	if (idx > get_file_max_index()) {
		log_err("BUG");
		return -EINVAL;
	}

	data = extend_read(get_data_queue(), inode->dirent->i_ino, &b);
	if (IS_ERR(data))
		return PTR_ERR(data);

	p_index = (uint32_t *) data;
	p_index += idx;
	data_no = le32_to_cpu(*p_index);

	data = extend_read(get_data_queue(), data_no, bp);
	extend_put(b);
	if (IS_ERR(data))
		return PTR_ERR(data);

	return 0;
}

static int __alloc_ebuf_by_file_idx(struct inode_info *inode, int idx, struct extend_buf **bp)
{
	char *data;
	struct extend_buf *b;
	uint32_t *p_index, data_no;
	int ret;

	if (idx > get_file_max_index()) {
		log_err("BUG");
		return -EINVAL;
	}

	data = extend_read(get_data_queue(), inode->dirent->i_ino, &b);
	if (IS_ERR(data))
		return PTR_ERR(data);

	p_index = (uint32_t *) data;
	p_index += idx;

	ret = alloc_extend_bitmap(&data_no);
	if (ret) {
		extend_put(b);
		return ret;
	}
	data = extend_new(get_data_queue(), data_no, bp);
	if (IS_ERR(data)) {
		extend_put(b);
		return PTR_ERR(data);
	}

	*p_index = cpu_to_le32(data_no);
	extend_mark_dirty(b);
	extend_write_dirty(b);
	extend_put(b);

	return 0;
}

int __vbfs_read_buf(struct inode_info *inode, char *buf, size_t size, off_t offset)
{
	int buf_size, index = -1, ret = 0, need_cache;
	off_t buf_off, tocopy;
	struct extend_buf *b;
	char *data, *pos, *buf_pos;
	size_t rd_len = 0;

	if (inode->dirent->i_size < offset)
		return -EINVAL;

	buf_off = offset + get_file_idx_size();
	buf_size = (size + offset < inode->dirent->i_size) ?
			size : (inode->dirent->i_size - offset);

	while (buf_size > 0) {
		if (buf_off < get_extend_size()) {
			data = extend_read(get_data_queue(), inode->dirent->i_ino, &b);
			if (IS_ERR(data))
				return PTR_ERR(data);
		} else {
			index = buf_off / get_extend_size() - 1;
			ret = __rd_ebuf_by_file_idx(inode, index, &b);
			if (ret)
				return ret;
			data = b->data;
		}

		if (buf_size < get_extend_size() - buf_off % get_extend_size()) {
			need_cache = 1;
			tocopy = buf_size;
		} else {
			if (buf_off < get_extend_size())
				need_cache = 1;
			else
				need_cache = 0;
			tocopy = get_extend_size() - buf_off % get_extend_size();
		}

		pos = data + buf_off % get_extend_size();
		buf_pos = buf + rd_len;
		memcpy(buf_pos, pos, tocopy);

		if (need_cache)
			extend_put(b);
		else
			extend_release(b);

		buf_off += tocopy;
		buf_size -= tocopy;
		rd_len += tocopy;
	}

	return rd_len;
}

int vbfs_read_buf(struct inode_info *inode, char *buf, size_t size, off_t offset)
{
	int ret;

	pthread_mutex_lock(&inode->lock);
	ret = __vbfs_read_buf(inode, buf, size, offset);
	pthread_mutex_unlock(&inode->lock);

	return ret;
}

static int is_need_alloc(uint64_t size, off_t buf_off)
{
	if (size > (buf_off - get_file_idx_size()))
		return 0;
	if (buf_off % get_extend_size())
		return 0;

	return 1;
}

int __vbfs_write_buf(struct inode_info *inode, const char *buf, size_t size, off_t offset)
{
	int buf_size, index = -1, ret = 0, fill;
	off_t buf_off, tocopy;
	struct extend_buf *b;
	char *data, *pos;
	const char *buf_pos;
	size_t wt_len = 0;
	uint64_t max_size;

	//log_dbg("size %u, offset %llu", size, offset);

	max_size = (__u64) get_file_max_index() * get_extend_size();

	if (inode->dirent->i_size < offset || offset > max_size)
		return -EINVAL;

	buf_off = offset + get_file_idx_size();
	if (offset + size > max_size)
		buf_size = max_size - offset - size;
	else
		buf_size = size;

	while (buf_size > 0) {
		if (buf_off < get_extend_size()) {
			data = extend_read(get_data_queue(), inode->dirent->i_ino, &b);
			if (IS_ERR(data))
				return PTR_ERR(data);
		} else {
			index = buf_off / get_extend_size() - 1;
			if (is_need_alloc(inode->dirent->i_size, buf_off))
				ret = __alloc_ebuf_by_file_idx(inode, index, &b);
			else
				ret = __rd_ebuf_by_file_idx(inode, index, &b);
			if (ret)
				return ret;
			data = b->data;
		}

		if (buf_size < get_extend_size() - buf_off % get_extend_size()) {
			tocopy = buf_size;
			fill = 0;
		} else {
			tocopy = get_extend_size() - buf_off % get_extend_size();
			if (buf_off < get_extend_size())
				fill = 0;
			else
				fill = 1;
		}

		pos = data + buf_off % get_extend_size();
		buf_pos = buf + wt_len;
		memcpy(pos, buf_pos, tocopy);

		extend_mark_dirty(b);

		if (fill) {
			extend_write_dirty(b);
			extend_release(b);
		} else
			extend_put(b);

		buf_off += tocopy;
		buf_size -= tocopy;
		wt_len += tocopy;

		if (inode->dirent->i_size < wt_len + offset) {
			inode->dirent->i_size = wt_len + offset;
			inode->status = DIRTY;
		}
	}

	//log_err("i_size %u, write size %u", inode->dirent->i_size, wt_len + offset);

	return wt_len;
}

int vbfs_write_buf(struct inode_info *inode, const char *buf, size_t size, off_t offset)
{
	int ret;

	pthread_mutex_lock(&inode->lock);
	ret = __vbfs_write_buf(inode, buf, size, offset);
	pthread_mutex_unlock(&inode->lock);

	return ret;
}
