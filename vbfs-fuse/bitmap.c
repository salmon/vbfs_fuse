#include "err.h"
#include "log.h"
#include "vbfs-fuse.h"

static pthread_mutex_t bitmap_lock = PTHREAD_MUTEX_INITIALIZER;

void init_bitmap(struct vbfs_bitmap *bitmap, uint32_t total_bits)
{
	bitmap->max_bit = total_bits;
	if (total_bits % BITS_PER_UNIT)
		bitmap->map_len = total_bits / BITS_PER_UNIT + 1;
	else
		bitmap->map_len = total_bits / BITS_PER_UNIT;
}

static void save_bitmap_header(bitmap_header_dk_t *bm_hd_dk, struct bitmap_header *bm_hd)
{
	memset(bm_hd_dk, 0, sizeof(bm_hd_dk));

	bm_hd_dk->bitmap_dk.group_no = cpu_to_le32(bm_hd->group_no);
	bm_hd_dk->bitmap_dk.total_cnt = cpu_to_le32(bm_hd->total_cnt);
	bm_hd_dk->bitmap_dk.free_cnt = cpu_to_le32(bm_hd->free_cnt);
	bm_hd_dk->bitmap_dk.current_position = cpu_to_le32(bm_hd->current_position);
}

static void load_bitmap_header(bitmap_header_dk_t *bm_hd_dk, struct bitmap_header *bm_hd)
{
	bm_hd->group_no = le32_to_cpu(bm_hd_dk->bitmap_dk.group_no);
	bm_hd->total_cnt = le32_to_cpu(bm_hd_dk->bitmap_dk.total_cnt);
	bm_hd->free_cnt = le32_to_cpu(bm_hd_dk->bitmap_dk.free_cnt);
	bm_hd->current_position = le32_to_cpu(bm_hd_dk->bitmap_dk.current_position);
}

static int __alloc_bitmap_by_ebuf(struct extend_buf *b)
{
	int ret, pos;
	struct vbfs_bitmap bm;
	char *buf;
	struct bitmap_header bm_header;

	buf = b->data;
	load_bitmap_header((bitmap_header_dk_t *) buf, &bm_header);
	if (0 == bm_header.free_cnt)
		return -1;

	init_bitmap(&bm, bm_header.total_cnt);
	bm.bitmap = (__u32 *)(buf + BITMAP_META_SIZE);

	pos = bm_header.current_position - 1;
	//log_dbg("pos %d, header pos %d, free_cnt %d", pos, bm_header.current_position, bm_header.free_cnt);
	ret = bitmap_next_clear_bit(&bm, pos);
	if (-1 != ret) {
		bm_header.free_cnt --;
		bm_header.current_position = ret;
		bitmap_set_bit(&bm, ret);
	} else
		bm_header.current_position = 0;

	save_bitmap_header((bitmap_header_dk_t *) buf, &bm_header);
	extend_mark_dirty(b);

	return ret;
}

static int __alloc_bitmap(uint32_t eno)
{
	struct extend_buf *b;
	char *data;
	int ret;

	data = extend_read(get_meta_queue(), eno, &b);
	if (IS_ERR(data))
		return PTR_ERR(data);

	ret = __alloc_bitmap_by_ebuf(b);

	extend_write_dirty(b);
	extend_put(b);

	return ret;
}

int __alloc_extend_bitmap(uint32_t *extend_no)
{
	int ret = 0;
	uint32_t start_no, curr_no;

	curr_no = get_bitmap_curr();
	start_no = curr_no;

	while (1) {
		ret = __alloc_bitmap(curr_no);
		//log_dbg("currno %u, %d\n", curr_no, ret);
		if (ret >= 0) {
			*extend_no = (curr_no - get_bitmap_offset()) * get_bitmap_capacity() + ret;
			return 0;
		}
		curr_no = add_bitmap_curr();
		if (start_no == curr_no)
			break;
	}

	ret = __alloc_bitmap(curr_no);
	//log_dbg("currno %u, %d\n", curr_no, ret);
	if (ret >= 0) {
		*extend_no = (curr_no - get_bitmap_offset())* get_bitmap_capacity() + ret;
		return 0;
	} else
		return -ENOSPC;
}

int alloc_extend_bitmap(uint32_t *extend_no)
{
	int ret;

	pthread_mutex_lock(&bitmap_lock);
	ret = __alloc_extend_bitmap(extend_no);
	pthread_mutex_unlock(&bitmap_lock);

	return ret;
}

int __free_extend_bitmap(const uint32_t extend_no, int sync)
{
	struct extend_buf *b;
	struct vbfs_bitmap bm;
	char *data;
	uint32_t data_no, offset;
	struct bitmap_header bm_header;

	data_no = extend_no / get_bitmap_capacity();
	offset = extend_no % get_bitmap_capacity();

	data = extend_read(get_meta_queue(), data_no, &b);
	if (IS_ERR(data))
		return PTR_ERR(data);

	load_bitmap_header((bitmap_header_dk_t *) data, &bm_header);

	init_bitmap(&bm, bm_header.total_cnt);
	bm.bitmap = (__u32 *)(data + BITMAP_META_SIZE);

	if (bitmap_clear_bit(&bm, offset)) {
		bm_header.free_cnt ++;
		save_bitmap_header((bitmap_header_dk_t *) data, &bm_header);
		extend_mark_dirty(b);
	}

	if (sync)
		extend_write_dirty(b);
	extend_put(b);

	return 0;
}

int free_extend_bitmap(const uint32_t extend_no)
{
	int ret;

	pthread_mutex_lock(&bitmap_lock);
	ret = __free_extend_bitmap(extend_no, 1);
	pthread_mutex_unlock(&bitmap_lock);

	return ret;
}

int free_extend_bitmap_async(const uint32_t extend_no)
{
	int ret;

	pthread_mutex_lock(&bitmap_lock);
	ret = __free_extend_bitmap(extend_no, 0);
	pthread_mutex_unlock(&bitmap_lock);

	return ret;
}

