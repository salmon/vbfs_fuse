#include "err.h"
#include "utils.h"
#include "super.h"
#include "extend.h"
#include "ioengine.h"

/* wait for the bit to be cleared when want to set it */
static void buffer_wait_on_bit_lock(struct extend_buf *b, int bit)
{
	pthread_mutex_lock(&b->lock);
	if (!test_and_set_bit(bit, &b->state)) {
		pthread_mutex_unlock(&b->lock);
		return;
	}
	do {
		pthread_cond_wait(&b->cond, &b->lock);
	} while (test_and_set_bit(bit, &b->state));
	pthread_mutex_unlock(&b->lock);
}

/* wait for the bit to be cleared */
static void buffer_wait_on_bit(struct extend_buf *b, int bit)
{
	pthread_mutex_lock(&b->lock);
	if (!test_bit(bit, &b->state)) {
		pthread_mutex_unlock(&b->lock);
		return;
	}
	do {
		pthread_cond_wait(&b->cond, &b->lock);
	} while (test_bit(bit, &b->state));
	pthread_mutex_unlock(&b->lock);
}

static void buffer_wakeup_bit(struct extend_buf *b, int bit)
{
	pthread_mutex_lock(&b->lock);
	pthread_cond_broadcast(&b->cond);
	pthread_mutex_unlock(&b->lock);
}

/*-------------------------------------------------------*/

static void queue_lock(struct queue *q)
{
	pthread_mutex_lock(&q->lock);
}

static int queue_trylock(struct queue *q)
{
	return pthread_mutex_trylock(&q->lock);
}

static void queue_unlock(struct queue *q)
{
	pthread_mutex_unlock(&q->lock);
}

static void *alloc_buffer_data(struct queue *q)
{
	return mp_valloc(get_extend_size());
}

static void free_buffer_data(struct queue *q, void *data)
{
	mp_free(data);
}

static struct extend_buf *alloc_buffer(struct queue *q)
{
	struct extend_buf *b = mp_malloc(sizeof(struct extend_buf));

	if (!b)
		return NULL;

	b->q = q;

	b->data = alloc_buffer_data(q);
	if (!b->data) {
		mp_free(b);
		return NULL;
	}

	return b;
}

static void free_buffer(struct extend_buf *b)
{
	struct queue *q = b->q;

	free_buffer_data(q, b->data);
	mp_free(b);
}

static void __link_buffer(struct extend_buf *b, uint32_t eno, int dirty)
{
	struct queue *q = b->q;

	q->n_buffers[dirty]++;
	b->eno = eno;
	b->list_mode = dirty;
	list_add(&b->lru_list, &q->lru[dirty]);
	hlist_add_head(&b->hash_list, &q->cache_hash[EXTNO_HASH(eno, q->hash_bits)]);
	b->last_accessed = get_curtime();
}

static void __unlink_buffer(struct extend_buf *b)
{
	struct queue *q = b->q;

	BUG_ON(!q->n_buffers[b->list_mode]);

	q->n_buffers[b->list_mode]--;
	hlist_del(&b->hash_list);
	list_del(&b->lru_list);
}

static void __relink_lru(struct extend_buf *b, int dirty)
{
	struct queue *q = b->q;

	BUG_ON(!q->n_buffers[b->list_mode]);

	q->n_buffers[b->list_mode]--;
	q->n_buffers[dirty]++;
	b->list_mode = dirty;
	list_move(&b->lru_list, &q->lru[dirty]);
}

static void submit_io(struct extend_buf *b, int rw, end_io_fn_t end_io)
{
	b->end_io_fn = end_io;
	b->rw = rw;
	b->real_eno = b->eno + b->q->eno_prefix;

	ioengine->io_submit(b);
}

static void read_endio(void *args)
{
	struct extend_buf *b = (struct extend_buf *) args;

	BUG_ON(!test_bit(B_READING, &b->state));

	clear_bit(B_READING, &b->state);

	buffer_wakeup_bit(b, B_READING);
}

static void write_endio(void *args)
{
	struct extend_buf *b = (struct extend_buf *) args;

	BUG_ON(!test_bit(B_WRITING, &b->state));

	clear_bit(B_WRITING, &b->state);

	buffer_wakeup_bit(b, B_WRITING);
}

static void __write_dirty_buffer(struct extend_buf *b)
{
	if (!test_bit(B_DIRTY, &b->state))
		return;

	clear_bit(B_DIRTY, &b->state);
	buffer_wait_on_bit_lock(b, B_WRITING);

	submit_io(b, WRITE, write_endio);
}

static void __make_buffer_clean(struct extend_buf *b)
{
	BUG_ON(b->hold_cnt);

	if (!b->state)
		return;

	buffer_wait_on_bit(b, B_READING);
	__write_dirty_buffer(b);
	buffer_wait_on_bit(b, B_WRITING);
}

static struct extend_buf *__get_unclaimed_buffer(struct queue *q)
{
	struct extend_buf *b;

	list_for_each_entry_reverse(b, &q->lru[LIST_CLEAN], lru_list) {
		BUG_ON(test_bit(B_WRITING, &b->state));
		BUG_ON(test_bit(B_DIRTY, &b->state));

		if (!b->hold_cnt) {
			__make_buffer_clean(b);
			__unlink_buffer(b);
			return b;
		}
	}

	list_for_each_entry_reverse(b, &q->lru[LIST_DIRTY], lru_list) {
		BUG_ON(test_bit(B_READING, &b->state));

		if (!b->hold_cnt) {
			__make_buffer_clean(b);
			__unlink_buffer(b);
			return b;
		}
	}

	return NULL;
}

static struct extend_buf *__find(struct queue *q, uint32_t eno)
{
	struct extend_buf *b;

	hlist_for_each_entry(b, &q->cache_hash[EXTNO_HASH(eno, q->hash_bits)],
				hash_list) {
		if (b->eno == eno) {
			return b;
		}
	}

	return NULL;
}

static void __wait_for_free_buffer(struct queue *q)
{
	queue_unlock(q);

	pthread_mutex_lock(&q->free_buffer_lock);
	pthread_cond_wait(&q->free_buffer_cond, &q->free_buffer_lock);
	pthread_mutex_unlock(&q->free_buffer_lock);

	queue_lock(q);
}

static void __free_buffer_wake(struct extend_buf *b)
{
	struct queue *q = b->q;

	if (!q->need_reserved_buffers)
		free_buffer(b);
	else {
		list_add(&b->lru_list, &q->reserved_buffers);
		q->need_reserved_buffers--;
	}

	pthread_mutex_destroy(&b->lock);
	pthread_cond_destroy(&b->cond);

	pthread_cond_signal(&q->free_buffer_cond);
}

static struct extend_buf *__alloc_buffer_wait(struct queue *q)
{
	struct extend_buf *b;

	while (1) {
		if (!list_empty(&q->reserved_buffers)) {
			b = list_entry(q->reserved_buffers.next,
				struct extend_buf, lru_list);
			list_del(&b->lru_list);
			q->need_reserved_buffers++;

			return b;
		}

		b = __get_unclaimed_buffer(q);
		if (b)
			return b;

		__wait_for_free_buffer(q);
	}
}

static void __write_dirty_buffers_async(struct queue *q)
{
	struct extend_buf *b, *tmp;

	list_for_each_entry_safe_reverse(b, tmp, &q->lru[LIST_DIRTY], lru_list) {
		BUG_ON(test_bit(B_READING, &b->state));

		if (!test_bit(B_DIRTY, &b->state) &&
		   !test_bit(B_WRITING, &b->state)) {
			__relink_lru(b, LIST_CLEAN);
			continue;
		}

		__write_dirty_buffer(b);
	}
}

/*-------------------------------------------------------*/

enum {
	NF_FRESH = 0,
	NF_READ = 1,
};

static struct extend_buf *__extend_new(struct queue *q, uint32_t eno,
			int nf, int *need_submit)
{
	struct extend_buf *b, *new_b = NULL;

	*need_submit = 0;

	b = __find(q, eno);
	if (b)
		goto found_buffer;

	new_b = __alloc_buffer_wait(q);
	if (!new_b)
		return NULL;

	/* 
 	 * mutex was unlocked, so need to recheck.
	 */
	b = __find(q, eno);
	if (b) {
		__free_buffer_wake(new_b);
		goto found_buffer;
	}
	b = new_b;
	b->hold_cnt = 1;
	b->error = 0;
	INIT_LIST_HEAD(&b->data_list);
	INIT_LIST_HEAD(&b->inode_list);
	pthread_mutex_init(&b->lock, NULL);
	pthread_cond_init(&b->cond, NULL);
	__link_buffer(b, eno, LIST_CLEAN);

	if (nf == NF_FRESH) {
		b->state = 0;
		return b;
	}

	b->state = 1 << B_READING;
	*need_submit = 1;

	return b;

found_buffer:
	b->hold_cnt++;
	__relink_lru(b, test_bit(B_DIRTY, &b->state) ||
		test_bit(B_WRITING, &b->state));

	return b;
}

static void *new_extend(struct queue *q, uint32_t eno, int nf, struct extend_buf **bp)
{
	int need_submit;
	struct extend_buf *b;

	queue_lock(q);
	b = __extend_new(q, eno, nf, &need_submit);
	queue_unlock(q);

	if (!b)
		return b;

	if (need_submit)
		submit_io(b, READ, read_endio);

	buffer_wait_on_bit(b, B_READING);

	if (b->error) {
		extend_release(b);
		return ERR_PTR(b->error);
	}

	*bp = b;

	return b->data;
}

void *extend_new(struct queue *q, uint32_t eno, struct extend_buf **bp)
{
	return new_extend(q, eno, NF_FRESH, bp);
}

void *extend_read(struct queue *q, uint32_t eno, struct extend_buf **bp)
{
	return new_extend(q, eno, NF_READ, bp);
}

void extend_mark_dirty(struct extend_buf *b)
{
	struct queue *q = b->q;

	queue_lock(q);

	BUG_ON(test_bit(B_READING, &b->state));
	if (!test_and_set_bit(B_DIRTY, &b->state))
		__relink_lru(b, LIST_DIRTY);

	queue_unlock(q);
}

int extend_write_dirty(struct extend_buf *b)
{
	struct queue *q = b->q;

	queue_lock(q);
	BUG_ON(test_bit(B_READING, &b->state));
	__write_dirty_buffer(b);
	queue_unlock(q);

	if (test_bit(B_WRITING, &b->state)) {
		buffer_wait_on_bit(b, B_WRITING);
	}

	queue_lock(q);
	if (!test_bit(B_DIRTY, &b->state) &&
	   !test_bit(B_WRITING, &b->state))
		__relink_lru(b, LIST_CLEAN);
	queue_unlock(q);

	return 0;
}

void extend_put(struct extend_buf *b)
{
	struct queue *q = b->q;

	queue_lock(q);

	BUG_ON(!b->hold_cnt);

	b->hold_cnt--;
	if (!b->hold_cnt) {
		pthread_cond_signal(&q->free_buffer_cond);
	}

	queue_unlock(q);
}

void extend_release(struct extend_buf *b)
{
	struct queue *q = b->q;

	queue_lock(q);

	BUG_ON(!b->hold_cnt);

	b->hold_cnt--;
	if (!b->hold_cnt) {
		pthread_cond_signal(&q->free_buffer_cond);

		if (!b->hold_cnt && !test_bit(B_READING, &b->state) &&
		   !test_bit(B_WRITING, &b->state) &&
		   !test_bit(B_DIRTY, &b->state)) {
			__unlink_buffer(b);
			__free_buffer_wake(b);
		}
	}

	queue_unlock(q);
}

/* used by flush */
int queue_write_dirty(struct queue *q)
{
	struct extend_buf *b, *tmp;

	queue_lock(q);
	__write_dirty_buffers_async(q);

	list_for_each_entry_safe_reverse(b, tmp, &q->lru[LIST_DIRTY], lru_list) {
		BUG_ON(test_bit(B_READING, &b->state));

		if (test_bit(B_WRITING, &b->state)) {
			buffer_wait_on_bit(b, B_WRITING);
		}

		if (!test_bit(B_DIRTY, &b->state) &&
		   !test_bit(B_WRITING, &b->state))
			__relink_lru(b, LIST_CLEAN);
	}
	queue_unlock(q);

	return 0;
}

void queue_write_dirty_async(struct queue *q)
{
	queue_lock(q);
	__write_dirty_buffers_async(q);
	queue_unlock(q);
}

static void drop_buffers(struct queue *q)
{
	struct extend_buf *b;
	int i;

	queue_write_dirty_async(q);

	queue_lock(q);

	while ((b = __get_unclaimed_buffer(q)))
		__free_buffer_wake(b);

	for (i = 0; i < LIST_SIZE; i++)
		BUG_ON(!list_empty(&q->lru[i]));

	queue_unlock(q);
}

static int __cleanup_old_buffer(struct extend_buf *b, unsigned long max_age)
{
	if (get_curtime() - b->last_accessed < max_age)
		return 1;

	if (b->hold_cnt)
		return 1;

	__make_buffer_clean(b);
	__unlink_buffer(b);
	__free_buffer_wake(b);

	return 0;
}

static void cleanup_old_buffers(struct queue *q)
{
	struct extend_buf *b;

	if (queue_trylock(q))
		return;

	while (!list_empty(&q->lru[LIST_CLEAN])) {
		b = list_entry(q->lru[LIST_CLEAN].prev,
			struct extend_buf, lru_list);
		if (__cleanup_old_buffer(b, MAX_AGE))
			break;
	}

	while (!list_empty(&q->lru[LIST_DIRTY])) {
		b = list_entry(q->lru[LIST_DIRTY].prev,
			struct extend_buf, lru_list);
		if (__cleanup_old_buffer(b, MAX_AGE))
			break;
	}

	queue_unlock(q);
}

static void *work_fn(void *args)
{
	struct queue *q = (struct queue *) args;
	while (1) {
		pthread_mutex_lock(&q->clean_lock);
		if (q->clean_stop) {
			pthread_mutex_unlock(&q->clean_lock);
			return NULL;
		}
		pthread_mutex_unlock(&q->clean_lock);

		cleanup_old_buffers(q);
		sleep(CLEANUP_INTERVAL);
	}
}

struct queue *queue_create(unsigned int reserved_buffers, int hash_bits, uint32_t eno_prefix)
{
	struct queue *q;
	int ret;
	unsigned int i;

	q = mp_malloc(sizeof(struct queue));
	if (!q) {
		ret = -ENOMEM;
		goto bad;
	}

	q->hash_bits = hash_bits;
	q->cache_hash = mp_malloc(sizeof(struct hlist_head) << hash_bits);
	if (!q->cache_hash) {
		ret = -ENOMEM;
		goto bad_hash;
	}

	for (i = 0; i < LIST_SIZE; i++) {
		INIT_LIST_HEAD(&q->lru[i]);
		q->n_buffers[i] = 0;
	}

	for (i = 0; i < 1 << hash_bits; i++)
		INIT_HLIST_HEAD(&q->cache_hash[i]);

	pthread_mutex_init(&q->lock, NULL);
	pthread_cond_init(&q->free_buffer_cond, NULL);
	pthread_mutex_init(&q->free_buffer_lock, NULL);

	INIT_LIST_HEAD(&q->reserved_buffers);
	q->need_reserved_buffers = reserved_buffers;

	while (q->need_reserved_buffers) {
		struct extend_buf *b = alloc_buffer(q);

		if (!b) {
			ret = -ENOMEM;
			goto bad_buffer;
		}
		__free_buffer_wake(b);
	}

	q->eno_prefix = eno_prefix;

	q->clean_stop = 0;
	pthread_mutex_init(&q->clean_lock, NULL);
	ret = pthread_create(&q->clean_tid, NULL, work_fn, q);
	if (ret < 0)
		goto bad_buffer;

	return q;

bad_buffer:
	while (!list_empty(&q->reserved_buffers)) {
		struct extend_buf *b = list_entry(q->reserved_buffers.next,
						 struct extend_buf, lru_list);
		list_del(&b->lru_list);
		free_buffer(b);
	}
bad_hash:
	mp_free(q);
bad:
	return ERR_PTR(ret);
}

void queue_destroy(struct queue *q)
{
	int i;

	pthread_mutex_lock(&q->clean_lock);
	q->clean_stop = 1;
	pthread_mutex_unlock(&q->clean_lock);

	pthread_join(q->clean_tid, NULL);
	pthread_mutex_destroy(&q->clean_lock);

	drop_buffers(q);

	for (i = 0; i < 1 << q->hash_bits; i++)
		BUG_ON(!hlist_empty(&q->cache_hash[i]));

	BUG_ON(q->need_reserved_buffers);

	while (!list_empty(&q->reserved_buffers)) {
		struct extend_buf *b = list_entry(q->reserved_buffers.next,
				struct extend_buf, lru_list);
		list_del(&b->lru_list);
		free_buffer(b);
	}

	for (i = 0; i < LIST_SIZE; i++)
		BUG_ON(q->n_buffers[i]);

	pthread_cond_destroy(&q->free_buffer_cond);
	pthread_mutex_destroy(&q->free_buffer_lock);
	pthread_mutex_destroy(&q->lock);

	mp_free(q->cache_hash);
	mp_free(q);
}

