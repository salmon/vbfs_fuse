#include "file.h"
#include "mempool.h"

extern vbfs_fuse_context_t vbfs_ctx;

static struct extend_content *alloc_extend(__u32 extend_no)
{
	struct extend_content *extend = NULL;
	__u32 extend_size = 0;

	extend = mp_malloc(sizeof(struct extend_content));
	if (NULL == extend)
		return NULL;

	extend_size = vbfs_ctx.super.s_extend_size;
	extend->extend_no = extend_no;
	extend->extend_buf = mp_valloc(extend_size);
	if (NULL == extend->extend_buf) {
		mp_free(extend, sizeof(struct extend_content));
		return NULL;
	}
	extend->extend_dirty = 0;
	extend->nref = 0;
	INIT_LIST_HEAD(&extend->data_list);

	return extend;
}

static int free_extend(struct extend_content *extend)
{
	__u32 extend_size = 0;

	extend_size = vbfs_ctx.super.s_extend_size;
	mp_free(extend->extend_buf, extend_size);
	mp_free(extend, sizeof(struct extend_content));

	return 0;
}

static struct extend_content *get_opened_extend(struct inode_vbfs *inode_v, __u32 extend_no)
{
	struct extend_content *extend = NULL;
	int found = 0;

	list_for_each_entry(extend, &inode_v->data_buf_list, data_list) {
		if (extend->extend_no == extend_no) {
			found = 1;
			break;
		}
	}

	if (found)
		return extend;
	else
		return NULL;
}

int open_extend_data_unlocked(struct inode_vbfs *inode_v, __u32 extend_no, int *err_no)
{
	struct extend_content *extend = NULL;

	extend = get_opened_extend(inode_v, extend_no);
	if (NULL != extend) {
		extend->nref ++;
		return 0;
	}

	extend = alloc_extend(extend_no);
	if (NULL == extend) {
		return -ENOMEM;
	}

	if (read_extend(extend_no, extend->extend_buf)) {
		free_extend(extend);
		return -EIO;
	}

	extend->nref = 1;
	list_add(&extend->data_list, &inode_v->data_buf_list);

	return 0;
}

int open_extend_data(struct inode_vbfs *inode_v, __u32 extend_no, int *err_no)
{
	int ret = 0;

	pthread_mutex_lock(&inode_v->lock_inode);
	ret = open_extend_data_unlocked(inode_v, extend_no, err_no);
	pthread_mutex_unlock(&inode_v->lock_inode);

	return ret;
}

int create_extend_data_unlocked(struct inode_vbfs *inode_v, __u32 extend_no, int *err_no)
{
	struct extend_content *extend = NULL;
	__u32 extend_size = 0;

	extend = get_opened_extend(inode_v, extend_no);
	if (NULL != extend) {
		return -EEXIST;
	}

	extend = alloc_extend(extend_no);
	if (NULL == extend) {
		return -ENOMEM;
	}

	extend_size = vbfs_ctx.super.s_extend_size;
	memset(extend->extend_buf, 0, extend_size);

	extend->nref = 1;
	extend->extend_dirty = 1;
	list_add(&extend->data_list, &inode_v->data_buf_list);

	return 0;
}

int create_extend_data(struct inode_vbfs *inode_v, __u32 extend_no, int *err_no)
{
	int ret = 0;

	pthread_mutex_lock(&inode_v->lock_inode);
	ret = create_extend_data_unlocked(inode_v, extend_no, err_no);
	pthread_mutex_unlock(&inode_v->lock_inode);

	return 0;
}

int release_extend_data(struct inode_vbfs *inode_v, __u32 extend_no)
{
	return 0;
}

int sync_extend_data(struct inode_vbfs *inode_v)
{
	return 0;
}

