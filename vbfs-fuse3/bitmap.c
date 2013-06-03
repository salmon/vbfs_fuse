#include "log.h"
#include "mempool.h"
#include "vbfs-fuse.h"

#define UNIT_SIZE sizeof(__u32)
#define BITS_PER_UNIT (UNIT_SIZE * CHAR_BIT)
#define UNIT_OFFSET(b) ((b) / BITS_PER_UNIT)
#define BIT_OFFSET(b) ((b) % BITS_PER_UNIT)
#define BIT_VALUE(b) ((__u32) 1 << BIT_OFFSET(b))

extern vbfs_fuse_context_t vbfs_ctx;

int bitmap_set_bit(struct vbfs_bitmap *bitmap, size_t bit)
{
	__u32 *bm = NULL;
	__u32 value = 0;

	if (bitmap->max_bit <= bit)
		return -1;

	bm = bitmap->bitmap;
	bm += UNIT_OFFSET(bit);

	value = le32_to_cpu(*bm);
	value |= BIT_VALUE(bit);
	*bm = le32_to_cpu(value);

	return 0;
}

int bitmap_clear_bit(struct vbfs_bitmap *bitmap, size_t bit)
{
	__u32 *bm = NULL;
	__u32 value = 0;

	if (bitmap->max_bit <= bit)
		return -1;

	bm = bitmap->bitmap;
	bm += UNIT_OFFSET(bit);

	value = le32_to_cpu(*bm);
	value &= ~BIT_VALUE(bit);
	*bm = le32_to_cpu(value);

	return 0;
}

static int bitmap_is_set(struct vbfs_bitmap *bitmap, size_t bit)
{
	__u32 *bm = NULL;
	__u32 value = 0;

	bm = bitmap->bitmap;
	bm += UNIT_OFFSET(bit);

	value = le32_to_cpu(*bm);

	return !!(value & BIT_VALUE(bit));
}

int bitmap_get_bit(struct vbfs_bitmap *bitmap, size_t bit, int *result)
{
	if (bitmap->max_bit <= bit)
		return -1;

	*result = bitmap_is_set(bitmap, bit);
	return 0;
}

void bitmap_set_all(struct vbfs_bitmap *bitmap)
{
	__u32 *bm = NULL;
	__u32 value = 0;
	int tail = bitmap->max_bit % BITS_PER_UNIT;

	memset(bitmap->bitmap, 0xff,
		bitmap->map_len * (BITS_PER_UNIT / CHAR_BIT));

	if (tail) {
		bm = bitmap->bitmap;
		bm += bitmap->map_len - 1;

		value = le32_to_cpu(*bm);
		value &= (__u32) -1 >> (BITS_PER_UNIT - tail);
		*bm = cpu_to_le32(value);
	}
}

void bitmap_clear_all(struct vbfs_bitmap *bitmap)
{
	memset(bitmap->bitmap, 0,
		bitmap->map_len * (BITS_PER_UNIT / CHAR_BIT));
}

int bitmap_is_all_set(struct vbfs_bitmap *bitmap)
{
	int i;
	int unused_bits;
	size_t size;

	unused_bits = bitmap->map_len * BITS_PER_UNIT - bitmap->max_bit;

	size = bitmap->map_len;
	if (unused_bits > 0)
		size --;

	for (i = 0; i < size; i ++) {
		if (le32_to_cpu(bitmap->bitmap[i]) != -1)
			return -1;
	}

	if (unused_bits > 0) {
		if ((le32_to_cpu(bitmap->bitmap[size]) &
			(((__u32) 1 << (BITS_PER_UNIT - unused_bits)) - 1))
			!= (((__u32) 1 << (BITS_PER_UNIT - unused_bits)) - 1)) {
			return -1;
		}
	}

	return 0;
}

/*
 * @pos: the position after which to search for a set bit
 *
 * Returns the position of the found bit, or -1 if no bit found.
 * */
int bitmap_next_set_bit(struct vbfs_bitmap *bitmap, int pos)
{
	size_t nl;
	size_t nb;
	__u32 bits;

	if (pos < 0)
		pos = -1;

	pos++;

	if (pos >= bitmap->max_bit)
		return -1;

	nl = pos / BITS_PER_UNIT;
	nb = pos % BITS_PER_UNIT;

	bits = le32_to_cpu(bitmap->bitmap[nl]) & ~(((__u32) 1 << nb) - 1);

	while (bits == 0 && ++nl < bitmap->map_len) {
		bits = le32_to_cpu(bitmap->bitmap[nl]);
	}

	if (nl == bitmap->map_len - 1) {
		int tail = bitmap->max_bit % BITS_PER_UNIT;

		if (tail)
			bits &= (((__u32) 1 << tail) - 1);
	}
	if (bits == 0)
		return -1;

	return bitops_ffs(bits) - 1 + nl * BITS_PER_UNIT;
}

int bitmap_next_clear_bit(struct vbfs_bitmap *bitmap, int pos)
{
	size_t nl;
	size_t nb;
	unsigned long bits;

	if (pos < 0)
		pos = -1;

	pos++;

	if (pos >= bitmap->max_bit)
		return -1;

	nl = pos / BITS_PER_UNIT;
	nb = pos % BITS_PER_UNIT;

	bits = ~(le32_to_cpu(bitmap->bitmap[nl])) & ~(((__u32) 1 << nb) - 1);

	while (bits == 0 && ++nl < bitmap->map_len) {
		bits = ~(le32_to_cpu(bitmap->bitmap[nl]));
	}

	if (nl == bitmap->map_len - 1) {
		int tail = bitmap->max_bit % BITS_PER_UNIT;

		if (tail)
			bits &= (__u32) -1 >> (BITS_PER_UNIT - tail);
	}
	if (bits == 0)
		return -1;

	return bitops_ffs(bits) - 1 + nl * BITS_PER_UNIT;
}

static int count_one_bits(__u32 word)
{
	int i, ret = 0;
	int len = BITS_PER_UNIT;

	for (i = 0; i < len; i ++) {
		if (word & (1 << i))
			++ret;
	}

	return 0;
}

int bitmap_count_bits(struct vbfs_bitmap *bitmap)
{
	size_t i, size;
	int ret = 0;
	__u32 *bm = NULL;
	__u32 val = 0;
	int unused_bits = 0;

	size = bitmap->map_len;
	bm = bitmap->bitmap;

	unused_bits = bitmap->map_len * BITS_PER_UNIT - bitmap->max_bit;
	if (unused_bits > 0)
		size --;

	for (i = 1; i < size; i ++) {
		val = le32_to_cpu(*bm++);
		ret += count_one_bits(val);
	}

	if (unused_bits > 0) {
		val = *bm;
		for (i = 0; i < unused_bits; i ++) {
			if (val & (1 << i))
				++ret;
		}
	}

	return ret;
}

/*
 * begin to implement vbfs bitmap helper
 * */

enum {
	EXTEND_BM_ALLOC,
	EXTEND_BM_FREE,
	INODE_BM_ALLOC,
	INODE_BM_FREE,
};

typedef int (*bitmap_op_func_t)(struct extend_data *edata, __u32 *p_no);

typedef struct {
	int type;
	struct extend_queue *equeue;
	bitmap_op_func_t per_bm_op;
} bm_op_t;

static __u32 bits_per_extend = 0;

void vbfs_init_bitmap()
{
	bits_per_extend = (get_extend_size() - EXTEND_BITMAP_META_SIZE) * CHAR_BIT;
}

static void load_inode_bitmap(inode_bitmap_group_dk_t *bm_disk,
			struct inode_bitmap_info *bm_info)
{
	bm_info->group_no =
		le32_to_cpu(bm_disk->inode_bm_gp.group_no);
	bm_info->total_inode =
		le32_to_cpu(bm_disk->inode_bm_gp.total_inode);
	bm_info->free_inode =
		le32_to_cpu(bm_disk->inode_bm_gp.free_inode);
	bm_info->current_position =
		le32_to_cpu(bm_disk->inode_bm_gp.current_position);
}

static void save_inode_bitmap(inode_bitmap_group_dk_t *bm_disk,
			struct inode_bitmap_info *bm_info)
{
	bm_disk->inode_bm_gp.group_no =
		cpu_to_le32(bm_info->group_no);
	bm_disk->inode_bm_gp.total_inode =
		cpu_to_le32(bm_info->total_inode);
	bm_disk->inode_bm_gp.free_inode =
		cpu_to_le32(bm_info->free_inode);
	bm_disk->inode_bm_gp.current_position =
		cpu_to_le32(bm_info->current_position);
}

static void load_extend_bitmap(extend_bitmap_group_dk_t *bm_disk,
			struct extend_bitmap_info *bm_info)
{
	bm_info->group_no =
		le32_to_cpu(bm_disk->extend_bm_gp.group_no);
	bm_info->total_extend =
		le32_to_cpu(bm_disk->extend_bm_gp.total_extend);
	bm_info->free_extend =
		le32_to_cpu(bm_disk->extend_bm_gp.free_extend);
	bm_info->current_position =
		le32_to_cpu(bm_disk->extend_bm_gp.current_position);
}

static void save_extend_bitmap(extend_bitmap_group_dk_t *bm_disk,
			struct extend_bitmap_info *bm_info)
{
	bm_disk->extend_bm_gp.group_no =
		cpu_to_le32(bm_info->group_no);
	bm_disk->extend_bm_gp.total_extend =
		cpu_to_le32(bm_info->total_extend);
	bm_disk->extend_bm_gp.free_extend =
		cpu_to_le32(bm_info->free_extend);
	bm_disk->extend_bm_gp.current_position =
		cpu_to_le32(bm_info->current_position);
}

static void init_bitmap(struct vbfs_bitmap *bitmap, __u32 total_bits)
{
	bitmap->max_bit = total_bits;
	if (total_bits % BITS_PER_UNIT)
		bitmap->map_len = total_bits / BITS_PER_UNIT + 1;
	else
		bitmap->map_len = total_bits / BITS_PER_UNIT;
}

/*
 * Return 0 if success, -1 if not found free bit
 * */
static int extend_bm_op_alloc(struct extend_data *edata, __u32 *pextend_no)
{
	struct vbfs_bitmap bitmap;
	struct extend_bitmap_info bm_info;
	int bit = 0;
	char *pos = NULL;

	memset(&bm_info, 0, sizeof(bm_info));

	pos = edata->buf;
	load_extend_bitmap((extend_bitmap_group_dk_t *) pos, &bm_info);
	if (0 == bm_info.free_extend) {
		return -1;
	}

	init_bitmap(&bitmap, bm_info.total_extend);
	bitmap.bitmap = (__u32 *)(pos + EXTEND_BITMAP_META_SIZE);

	bit = bitmap_next_clear_bit(&bitmap, bm_info.current_position - 1);
	if (-1 == bit) {
		bm_info.current_position = 0;
		save_extend_bitmap((extend_bitmap_group_dk_t *) pos, &bm_info);
		edata->status = BUFFER_DIRTY;
		return -1;
	}

	bitmap_set_bit(&bitmap, bit);

	*pextend_no = bm_info.group_no * bits_per_extend + bit + vbfs_ctx.super.inode_offset;
	bm_info.free_extend --;
	bm_info.current_position = bit;
	pos = edata->buf;
	save_extend_bitmap((extend_bitmap_group_dk_t *) pos, &bm_info);
	edata->status = BUFFER_DIRTY;

	return 0;
}

static int inode_bm_op_alloc(struct extend_data *edata, __u32 *pinode_no)
{
	struct vbfs_bitmap bitmap;
	struct inode_bitmap_info bm_info;
	int bit = 0;
	char *pos = NULL;

	memset(&bm_info, 0, sizeof(bm_info));

	pos = edata->buf;
	load_inode_bitmap((inode_bitmap_group_dk_t *) pos, &bm_info);
	log_dbg("extend no %u, free inode %u", edata->extend_no, bm_info.free_inode);
	if (0 == bm_info.free_inode) {
		return -1;
	}

	init_bitmap(&bitmap, bm_info.total_inode);
	bitmap.bitmap = (__u32 *)(pos + INODE_BITMAP_META_SIZE);

	bit = bitmap_next_clear_bit(&bitmap, bm_info.current_position - 1);
	log_dbg("%d %u %u", bit, bm_info.current_position, bm_info.total_inode);
	if (-1 == bit) {
		bm_info.current_position = 0;
		save_inode_bitmap((inode_bitmap_group_dk_t *) pos, &bm_info);
		edata->status = BUFFER_DIRTY;
		return -1;
	}

	bitmap_set_bit(&bitmap, bit);

	*pinode_no = bm_info.group_no * bits_per_extend + bit;
	bm_info.free_inode --;
	log_dbg("extend no %u, free inode %u", edata->extend_no, bm_info.free_inode);
	bm_info.current_position = bit;
	pos = edata->buf;
	save_inode_bitmap((inode_bitmap_group_dk_t *) pos, &bm_info);
	edata->status = BUFFER_DIRTY;

	return 0;
}

static int extend_bm_op_free(struct extend_data *edata, __u32 *pextend_no)
{
	struct vbfs_bitmap bitmap;
	struct extend_bitmap_info bm_info;
	__u32 ext_no = *pextend_no;
	char *pos = NULL;

	memset(&bm_info, 0, sizeof(bm_info));
	if (ext_no < vbfs_ctx.super.inode_offset) {
		log_err("BUG");
	}
	ext_no -= vbfs_ctx.super.inode_offset;

	pos = edata->buf;
	load_extend_bitmap((extend_bitmap_group_dk_t *) pos, &bm_info);
	if (bm_info.free_extend == bm_info.total_extend) {
		log_err("BUG");
		return -1;
	}

	init_bitmap(&bitmap, bm_info.total_extend);
	bitmap.bitmap = (__u32 *)(pos + EXTEND_BITMAP_META_SIZE);

	bitmap_clear_bit(&bitmap, ext_no);

	bm_info.free_extend ++;
	save_extend_bitmap((extend_bitmap_group_dk_t *) pos, &bm_info);
	edata->status = BUFFER_DIRTY;

	return 0;
}

static int inode_bm_op_free(struct extend_data *edata, __u32 *pinode_no)
{
	struct vbfs_bitmap bitmap;
	struct inode_bitmap_info bm_info;
	const __u32 ino = *pinode_no;
	char *pos = NULL;

	memset(&bm_info, 0, sizeof(bm_info));

	log_dbg("%u", *pinode_no);

	pos = edata->buf;
	load_inode_bitmap((inode_bitmap_group_dk_t *) pos, &bm_info);
	if (bm_info.free_inode == bm_info.total_inode) {
		log_err("BUG");
		return -1;
	}

	init_bitmap(&bitmap, bm_info.total_inode);
	bitmap.bitmap = (__u32 *)(pos + INODE_BITMAP_META_SIZE);

	bitmap_clear_bit(&bitmap, ino);

	bm_info.free_inode ++;
	save_inode_bitmap((inode_bitmap_group_dk_t *) pos, &bm_info);

	return 0;
}

/*
 * Returns 0 if success, less than 0 if some error found
 */
static int bitmap_operation(const __u32 bm_extno, bm_op_t *bm_op, __u32 *p_no)
{
	int ret = 0;
	struct extend_data *edata = NULL;

	edata = open_edata(bm_extno, bm_op->equeue, &ret);
	if (ret)
		return ret;

	read_edata(edata);
	if (BUFFER_NOT_READY != edata->status) {
		pthread_mutex_lock(&edata->ed_lock);
		ret = bm_op->per_bm_op(edata, p_no);
		pthread_mutex_unlock(&edata->ed_lock);
		if (ret) {
			ret = -ENOSPC;
		}
	} else {
		return -EIO;
	}

	close_edata(edata);

	return ret;
}

int alloc_extend_bitmap(__u32 *pextend_no)
{
	__u32 bm_curr_extno, bm_start_extno;
	bm_op_t bm_op;
	int ret = 0;

	bm_op.type = EXTEND_BM_ALLOC;
	bm_op.equeue = &vbfs_ctx.extend_bm_queue;
	bm_op.per_bm_op = extend_bm_op_alloc;

	bm_curr_extno = get_extend_bm_curr();
	bm_start_extno = bm_curr_extno;

	while (1) {
		ret = bitmap_operation(bm_curr_extno, &bm_op, pextend_no);
		if (0 == ret) {
			return 0;
		}
		if (bm_start_extno == add_extend_bm_curr()) {
			break;
		}
		bm_curr_extno = get_extend_bm_curr();
	}

	ret = bitmap_operation(bm_curr_extno, &bm_op, pextend_no);
	if (0 == ret)
		return 0;
	else
		return ret;
}

int alloc_inode_bitmap(__u32 *pinode_no)
{
	__u32 bm_curr_extno, bm_start_extno;
	bm_op_t bm_op;
	int ret = 0;

	bm_op.type = INODE_BM_ALLOC;
	bm_op.equeue = &vbfs_ctx.inode_bm_queue;
	bm_op.per_bm_op = inode_bm_op_alloc;

	bm_curr_extno = get_inode_bm_curr();
	bm_start_extno = bm_curr_extno;

	while (1) {
		ret = bitmap_operation(bm_curr_extno, &bm_op, pinode_no);
		if (0 == ret) {
			return 0;
		}
		if (bm_start_extno == add_inode_bm_curr()) {
			break;
		}
		bm_curr_extno = get_inode_bm_curr();
	}

	ret = bitmap_operation(bm_curr_extno, &bm_op, pinode_no);
	if (0 == ret)
		return 0;
	else
		return -ENOSPC;
}

int free_extend_bitmap(const __u32 extend_no)
{
	__u32 bm_curr_extno = 0, ext_no = 0;
	bm_op_t bm_op;
	int ret = 0;

	memset(&bm_op, 0, sizeof(bm_op));
	bm_op.type = EXTEND_BM_FREE;
	bm_op.equeue = &vbfs_ctx.extend_bm_queue;
	bm_op.per_bm_op = extend_bm_op_free;

	bm_curr_extno = extend_no / bits_per_extend;
	ext_no = extend_no % bits_per_extend;

	ret = bitmap_operation(bm_curr_extno, &bm_op, &ext_no);

	return ret;
}

/*
int free_extends(struct inode_vbfs *inode_v)
{
	bm_op_t bm_op;

	bm_op.type = EXTEND_BM_FREE;
	bm_op.equeue = &vbfs_ctx.extend_bm_queue;
	bm_op.per_bm_op = extend_bm_op_burst_free;

	return 0;
}
*/

int free_inode_bitmap(const __u32 inode_no)
{
	__u32 bm_curr_extno = 0, ino = 0;
	bm_op_t bm_op;
	int ret = 0;

	memset(&bm_op, 0, sizeof(bm_op));
	bm_op.type = INODE_BM_FREE;
	bm_op.equeue = &vbfs_ctx.inode_bm_queue;
	bm_op.per_bm_op = inode_bm_op_free;

	bm_curr_extno = inode_no / bits_per_extend;
	ino = inode_no % bits_per_extend;

	ret = bitmap_operation(bm_curr_extno, &bm_op, &ino);

	return ret;
}
