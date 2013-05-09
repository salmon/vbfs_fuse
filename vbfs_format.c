#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <uuid/uuid.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <assert.h>

#include "vbfs_format.h"

struct vbfs_paramters vbfs_params;
struct vbfs_superblock vbfs_superblk;

static void vbfs_init_paramters()
{
	vbfs_params.extend_size_kb = 0;
	vbfs_params.dev_name = NULL;
	vbfs_params.total_size = 0;
	vbfs_params.file_idx_len = 0;

	vbfs_params.inode_ratio = 1;
	vbfs_params.bad_ratio = 2048;

	vbfs_params.inode_extend_cnt = 0;

	vbfs_params.fd = -1;
}

static void cmd_usage()
{
	fprintf(stderr, "Usage: mkfs_vbfs [options] device\n");
	fprintf(stderr, "[options]\n");
	fprintf(stderr, "-e assign extend size in KB\n");
	fprintf(stderr, "-i assign inode nums ratio of extend nums\n");
	fprintf(stderr, "\t\tdefaut 1:1\n");
	fprintf(stderr, "-b assign bad ratio of extend nums\n");
	fprintf(stderr, "\t\tdefaut 1:2048\n");
	fprintf(stderr, "-x assign file index size of first extend in KB\n");
	fprintf(stderr, "\t\tdefaut 256KB\n");
	exit(1);
}

static int is_raid_friendly()
{
#if 0
	int fd, len;
	char buf[1024];

	fd = open("/proc/mdstat", O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open /proc/mdstat error\n");
		return 0;
	}
	while (true) {
		len = read(fd, buf, sizeof(buf) - 1);
		if (len <= 0) {
			break;
		} else {
			buf[len] = 0;
			vbfs_params.dev_name in buf;
		}
	}
	
	return 1;
#endif
	int fd;
	char name[8], buf[32], attr[128];
	int chunk_size, raid_disks, stripe_size_kb;

	name[7] = 0;
	buf[31] = 0;
	/* not good */
	strncpy(name, vbfs_params.dev_name + 5, sizeof(name) - 1);
	if (0 == strncmp(name, "md", 2)) {
		snprintf(attr, sizeof(attr), "/sys/block/%s/md/chunk_size", name);
		fd = open(attr, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "open %s failed\n", attr);
			return 1;
		}
		read(fd, buf, sizeof(buf) - 1);
		chunk_size = atoi(buf);

		snprintf(attr, sizeof(attr), "/sys/block/%s/md/raid_disks", name);
		fd = open(attr, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "open %s failed\n", attr);
			return 1;
		}
		read(fd, buf, sizeof(buf) - 1);
		raid_disks = atoi(buf);

		stripe_size_kb = (raid_disks - 1) * chunk_size / 1024;
		if (vbfs_params.extend_size_kb == stripe_size_kb) {
			return 0;
		} else {
			return 1;
		}
	}
	return 0;
}

static int get_device_info()
{
	int fd = 0;
	struct stat stat_buf;

	fd = open(vbfs_params.dev_name, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Can't open %s\n", vbfs_params.dev_name);
		return -1;
	}
	vbfs_params.fd = fd;

	if (fstat(fd, &stat_buf) < 0) {
		fprintf(stderr, "Can't get %s stat\n", vbfs_params.dev_name);
		return -1;
	} 

	if (S_ISREG(stat_buf.st_mode)) {
		vbfs_params.total_size = stat_buf.st_size;
	} else if (S_ISBLK(stat_buf.st_mode)) {
		if (ioctl(fd, BLKGETSIZE, &vbfs_params.total_size) < 0) {
			fprintf(stderr, "Can't get disk size\n");
		}
		if (0 != is_raid_friendly()) {
			fprintf(stderr, "extend size is not a full stripe size\n");
		}
	} else {
		fprintf(stderr, "%s is not a block device\n", vbfs_params.dev_name);
		return -1;
	}

	return 0;
}

static void parse_options(int argc, char **argv)
{
	static const char *option_string = "e:i:b:x";
	int option = 0;

	while ((option = getopt(argc, argv, option_string)) != EOF) {
		switch (option) {
			case 'e':
				vbfs_params.extend_size_kb = atoi(optarg);
				break;
			case 'i':
				vbfs_params.inode_ratio = atoi(optarg);
				break;
			case 'b':
				vbfs_params.bad_ratio = atoi(optarg);
				break;
			case 'x':
				vbfs_params.file_idx_len = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Unknown option %c\n", option);
				cmd_usage();
				break;
		}
	}

	/* check paramters */
	if (vbfs_params.extend_size_kb < 512) {
		fprintf(stderr, "extend size must bigger than 512KB\n");
		cmd_usage();
	}
	if (vbfs_params.extend_size_kb > 8192) {
		fprintf(stderr, "\nWARN: extend size exceeds 8M\n\n");
	}

	if (vbfs_params.file_idx_len > vbfs_params.extend_size_kb) {
		fprintf(stderr, "file index of first extend is too large\n");
		cmd_usage();
	}

	if ((optind + 1) != argc) {
		cmd_usage();
	}

	vbfs_params.dev_name = argv[optind];
}

__u32 calc_div(__u32 dividend, __u32 divisor)
{
	__u32 result = 0;

	if (dividend % divisor) {
		result = dividend / divisor + 1;
	} else {
		result = dividend / divisor;
	}

	return result;
}

static int vbfs_prepare_superblock()
{
	__u32 extend_size = 0;
	__u64 disk_size = 0;

	__u32 extend_count = 0;
	__u32 bad_max_count = 0;
	__u32 bad_extend_count = 0;
	__u32 inode_count = 0;
	__u32 inode_bitmap_cnt = 0;
	__u32 inode_bitmap_off_t = 0;
	__u32 extend_bitmap_cnt = 0;
	__u32 extend_bitmap_off_t = 0;

	__u32 bm_capacity = 0;

	extend_size = vbfs_params.extend_size_kb * 1024;
	disk_size = vbfs_params.total_size;

	printf("disk size %llu, extend size %u\n", disk_size, extend_size);

	/* extend_count */
	extend_count = disk_size / extend_size;

	printf("extend count %u\n", extend_count);

	/* bad extend (record bad extend number)*/
	bad_max_count = calc_div(extend_count, vbfs_params.bad_ratio);
	bad_extend_count = calc_div(bad_max_count, extend_size / 8);

	printf("bad use %u extends\n", bad_extend_count);

	inode_count = calc_div(extend_count, vbfs_params.inode_ratio);
	vbfs_params.inode_extend_cnt = calc_div(inode_count * INODE_SIZE, extend_size);
	inode_count = vbfs_params.inode_extend_cnt * (extend_size / INODE_SIZE);

	printf("inode total num is %u, use %u extends\n",
			inode_count, vbfs_params.inode_extend_cnt);

	/* inode bitmap */
	inode_bitmap_off_t = bad_extend_count + 1;
	bm_capacity = (extend_size - INODE_BITMAP_META_SIZE) * 8;
	inode_bitmap_cnt = calc_div(inode_count, bm_capacity);

	printf("inode bitmap use %u extend, position at %u extend\n",
			inode_bitmap_cnt, inode_bitmap_off_t);

	/* extend bitmap */
	extend_bitmap_off_t = inode_bitmap_cnt + inode_bitmap_off_t;
	bm_capacity = (extend_size - EXTEND_BITMAP_META_SIZE) * 8;
	extend_bitmap_cnt = calc_div(extend_count, bm_capacity);

	printf("extend bitmap use %u extend, position at %u extend\n", 
			extend_bitmap_cnt, extend_bitmap_off_t);

	memset(&vbfs_superblk, 0, sizeof(vbfs_superblk));

	/* 
 	 * generate superblock 
 	 * */
	vbfs_superblk.s_magic = VBFS_SUPER_MAGIC;
	vbfs_superblk.s_extend_size = extend_size;
	vbfs_superblk.s_extend_count = extend_count;
	vbfs_superblk.s_inode_count = inode_count;
	vbfs_superblk.s_file_idx_len = vbfs_params.file_idx_len;

	/* bad extend */
	vbfs_superblk.bad_count = 0;
	vbfs_superblk.bad_extend_count = bad_extend_count;
	vbfs_superblk.bad_extend_current = 0;
	/* first extend is used by superblock*/
	vbfs_superblk.bad_extend_offset = 1;

	/* extend bitmap */
	vbfs_superblk.extend_bitmap_count = extend_bitmap_cnt;
	vbfs_superblk.extend_bitmap_current = 0;
	vbfs_superblk.extend_bitmap_offset = extend_bitmap_off_t;

	/* inode bitmap */
	vbfs_superblk.inode_bitmap_count = inode_bitmap_cnt;
	vbfs_superblk.inode_bitmap_offset = inode_bitmap_off_t;
	vbfs_superblk.inode_bitmap_current = 0;

	vbfs_superblk.s_ctime = time(NULL);
	vbfs_superblk.s_mount_time = 0;
	/* 0 represent clean, 1 unclean */
	vbfs_superblk.s_state = 0;

	uuid_generate(vbfs_superblk.uuid);

	return 0;
}

/* 
 * 0 -> not found
 * */
__u32 get_first_s_bit(char *bitmap, int bm_size)
{
	int val;
        __u32 *bm = 0;
	int i = 0, pos = 0;

        bm = (__u32 *) bitmap;

	for (i = 0; i < bm_size / 32; i ++) {
        	val = *bm;

		if (ffs(val) == 0) {
			bm ++;
			pos += 32;
		} else {
			pos = pos + ffs(val) - 1;
			return pos;
		}
	}

	return bm_size;
}

__u32 get_val(__u32 bit)
{
	__u32 val = 0;
	__u32 tmp = 0;

	if (bit == 0)
		return ~val;

	assert(! (bit > 64));

	tmp = bit % 4;

	switch (tmp) {
	case 3:
		val |= 0x7;
		break;
	case 2:
		val |= 0x3;
		break;
	case 1:
		val |= 0x1;
		break;
	case 0:
		val |= 0xF;
		break;
	}

	while (bit > 4) {
		bit -= 4;
		val <<= 4;
		val |= 0xF;
	}

	return ~val;
}

static void init_bitmap(char *bitmap, __u32 bitmap_size, __u32 total_inode)
{
	__u32 val = 0;
	__u32 m = 0, n = 0;
	char *pos = NULL;
	__u32 remain = 0;
	__u32 *bm = 0;

	assert(! (total_inode > bitmap_size));
	assert(! (bitmap_size % 32));

	if (bitmap_size == total_inode)
		return;

	if (total_inode == 0) {
		memset(bitmap, 0xff, bitmap_size / 8);
		return;
	}

	bm = (__u32 *) bitmap;

	m = total_inode % 32;
	n = total_inode / 32;

	bm += n;
	val = le32_to_cpu(*bm);
	val |= get_val(m);
	*bm = cpu_to_le32(val);

	remain = (bitmap_size - total_inode) / 32;
	if (remain) {
		pos = bitmap + bitmap_size / 8 - remain * 4;
		memset(pos, 0xff, remain * 4);
	}
}

static void set_first_bits(char *bitmap, __u32 bit_num)
{
	__u32 *bm = 0;
	__u32 val = 0;
	__u32 i;

	bm = (__u32 *) bitmap;
	val = le32_to_cpu(*bm);

	for (i = 0; i < bit_num; i++)
		val |= 1 << i;

	*bm = cpu_to_le32(val);

}

static void inode_bm_meta_to_disk(struct inode_bitmap_group_disk *inode_bm_gp,
				struct inode_bitmap_group *inode_meta)
{
	inode_bm_gp->group_no = inode_meta->group_no;
	inode_bm_gp->total_inode = inode_meta->total_inode;
	inode_bm_gp->free_inode = inode_meta->free_inode;
	inode_bm_gp->current_position = inode_meta->current_position;
}

static void vbfs_inode_bm_prepare(__u32 group_no, __u32 total_inodes, char *inode_bm_extend)
{
	inode_bitmap_group_dk_t inode_bm_dk;
	struct inode_bitmap_group inode_meta;
	//char *inode_bm_extend = NULL;
	char *bitmap_region = NULL;
	__u32 extend_size;
	__u32 bitmap_size;

	extend_size = vbfs_params.extend_size_kb * 1024;

	memset(&inode_bm_dk, 0, sizeof(inode_bm_dk));
	/*
	if ((inode_bm_extend = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No memory\n");
		goto err;
	}
	*/
	memset(inode_bm_extend, 0, extend_size);

	bitmap_size = extend_size - INODE_BITMAP_META_SIZE;
	bitmap_region = inode_bm_extend + INODE_BITMAP_META_SIZE;

	inode_meta.free_inode = total_inodes;
	inode_meta.total_inode = total_inodes;
	inode_meta.group_no = group_no;
	inode_meta.current_position = 0;

	init_bitmap(bitmap_region, bitmap_size, total_inodes);

	if (group_no == 0) {
		inode_meta.free_inode --;
		inode_meta.current_position ++;
		/* one bit for root inode*/
		set_first_bits(bitmap_region, 1);
	}

	inode_bm_meta_to_disk(&inode_bm_dk.inode_bm_gp, &inode_meta);

	memcpy(inode_bm_extend, &inode_bm_dk, sizeof(inode_bm_dk));
}

static void extend_bm_meta_to_disk(struct extend_bitmap_group_disk *extend_bm_gp,
				struct extend_bitmap_group *extend_meta)
{
	extend_bm_gp->group_no = extend_meta->group_no;
	extend_bm_gp->total_extend = extend_meta->total_extend;
	extend_bm_gp->free_extend = extend_meta->free_extend;
	extend_bm_gp->current_position = extend_meta->current_position;
}

static void vbfs_extend_bm_prepare(__u32 group_no, __u32 total_extends, char *extend_bm_extend)
{
	extend_bitmap_group_dk_t extend_bm_dk;
	struct extend_bitmap_group extend_meta;
	char *bitmap_region = NULL;

	__u32 extend_size;
	__u32 bitmap_size;

	extend_size = vbfs_params.extend_size_kb * 1024;

	memset(&extend_bm_dk, 0, sizeof(extend_bm_dk));
	memset(extend_bm_extend, 0, extend_size);

	bitmap_size = extend_size - EXTEND_BITMAP_META_SIZE;
	bitmap_region = extend_bm_extend + EXTEND_BITMAP_META_SIZE;

	extend_meta.free_extend = total_extends;
	extend_meta.total_extend = total_extends;
	extend_meta.group_no = group_no;
	extend_meta.current_position = 0;

	init_bitmap(bitmap_region, bitmap_size, total_extends);

	if (group_no == 0) {
		extend_meta.free_extend --;
		extend_meta.current_position ++;
		/* one bit for root dentry */
		set_first_bits(bitmap_region, 1);
	}

	extend_bm_meta_to_disk(&extend_bm_dk.extend_bm_gp, &extend_meta);

	memcpy(extend_bm_extend, &extend_bm_dk, sizeof(extend_bm_dk));
}

static void inode_to_disk(struct vbfs_inode_disk *inode_dk,
			struct vbfs_inode *inode)
{
	inode_dk->i_ino = cpu_to_le32(inode->i_ino);
	inode_dk->i_pino = cpu_to_le32(inode->i_pino);
	inode_dk->i_mode = cpu_to_le32(inode->i_mode);
	inode_dk->i_size = cpu_to_le64(inode->i_size);
	inode_dk->i_atime = cpu_to_le32(inode->i_atime);
	inode_dk->i_ctime = cpu_to_le32(inode->i_ctime);
	inode_dk->i_mtime = cpu_to_le32(inode->i_mtime);
	inode_dk->i_extend = cpu_to_le32(inode->i_extend);
}

static void prepare_root_inode(char *inode_extend)
{
	vbfs_inode_dk_t vbfs_inode_dk;
	struct vbfs_inode vbfs_root;

	memset(&vbfs_inode_dk, 0, sizeof(vbfs_inode_dk_t));

	vbfs_root.i_ino = 0;
	vbfs_root.i_pino = 0;
	vbfs_root.i_mode = VBFS_FT_DIR;
	vbfs_root.i_size = vbfs_params.extend_size_kb * 1024;
	vbfs_root.i_atime = time(NULL);
	vbfs_root.i_ctime = time(NULL);
	vbfs_root.i_mtime = time(NULL);
	vbfs_root.i_extend = 0;

	inode_to_disk(&vbfs_inode_dk.vbfs_inode, &vbfs_root);
	memcpy(inode_extend, &vbfs_inode_dk, sizeof(vbfs_inode_dk_t));
}

static void dir_meta_to_disk(struct vbfs_dir_meta_disk *dir_meta_dk,
				struct dir_metadata *dir_meta)
{
	dir_meta_dk->group_no = cpu_to_le32(dir_meta->group_no);
	dir_meta_dk->total_extends = cpu_to_le32(dir_meta->total_extends);

	dir_meta_dk->dir_self_count = cpu_to_le32(dir_meta->dir_self_count);
	dir_meta_dk->dir_total_count = cpu_to_le32(dir_meta->dir_total_count);

	dir_meta_dk->next_extend = cpu_to_le32(dir_meta->next_extend);
	dir_meta_dk->dir_capacity = cpu_to_le32(dir_meta->dir_capacity);
	dir_meta_dk->bitmap_size = cpu_to_le32(dir_meta->bitmap_size);
}

static void prepare_root_dentry(char *extend)
{
	int extend_size, dir_count;
	struct dir_metadata dir_meta;
	struct vbfs_dir_entry dot_and_dotdot;
	vbfs_dir_meta_dk_t dir_meta_dk;
	int len;
	char *pos = NULL, *dir_bitmap = NULL;

	extend_size = vbfs_params.extend_size_kb * 1024;
	len = sizeof(struct vbfs_dir_entry);
	memset(extend, 0, extend_size);

	/* init directory metadata */
	pos = extend;

	dir_meta.group_no = 0;
	dir_meta.total_extends = 1;
	dir_meta.dir_self_count = 2;
	dir_meta.next_extend = 0;
	dir_count = (extend_size - VBFS_DIR_META_SIZE) / VBFS_DIR_SIZE;
	dir_meta.bitmap_size = calc_div(dir_count, 512 * 8);
	dir_meta.dir_capacity = dir_count - dir_meta.bitmap_size;

	memset(&dir_meta_dk, 0, sizeof(vbfs_dir_meta_dk_t));
	dir_meta_to_disk(&dir_meta_dk.vbfs_dir_meta, &dir_meta);
	memcpy(pos, &dir_meta_dk, sizeof(vbfs_dir_meta_dk_t));

	/* init directory bitmap */
	pos = extend + VBFS_DIR_META_SIZE;

	dir_bitmap = extend + VBFS_DIR_META_SIZE;
	init_bitmap(dir_bitmap, dir_meta.bitmap_size * 4096, dir_meta.dir_capacity);
	set_first_bits(dir_bitmap, 2);

	/* init dot(.) */
	pos = extend + VBFS_DIR_META_SIZE + dir_meta.bitmap_size * 512;

	memset(&dot_and_dotdot, 0, sizeof(dot_and_dotdot));
	dot_and_dotdot.inode = 0;
	dot_and_dotdot.file_type = VBFS_FT_DIR;
	dot_and_dotdot.name[0] = '.';
	memcpy(pos, &dot_and_dotdot, len);

	/* init dotdot(..) */
	pos += len;

	memset(&dot_and_dotdot, 0, sizeof(dot_and_dotdot));
	dot_and_dotdot.inode = 0;
	dot_and_dotdot.file_type = VBFS_FT_DIR;
	dot_and_dotdot.name[0] = '.';
	dot_and_dotdot.name[1] = '.';
	memcpy(pos, &dot_and_dotdot, len);
}

int write_to_disk(int fd, void *buf, __u64 offset, size_t len)
{
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error %s\n", strerror(errno));
		return -1;
	}
	if (write(fd, buf, len) < 0) {
		fprintf(stderr, "write error %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int write_extend(__u32 extend_no, void *buf)
{
	size_t len = vbfs_params.extend_size_kb * 1024;
	int fd = vbfs_params.fd;
	off64_t offset = (__u64)extend_no * len;

	if (write_to_disk(fd, buf, offset, len)) {
		return -1;
	}

	return 0;
}

static int write_root_inode()
{
	int ret = 0;
	char *extend = NULL;
	int extend_size = 0;
	int extend_no = 0;

	extend_size = vbfs_params.extend_size_kb * 1024;

	if ((extend = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}

	extend_no = vbfs_superblk.extend_bitmap_offset
			+ vbfs_superblk.extend_bitmap_count;

	prepare_root_inode(extend);

	if (write_extend(extend_no, extend)) {
		free(extend);
		return -1;
	}

	free(extend);
	return ret;
}

static int write_root_dentry()
{
	int ret = 0;
	char *extend = NULL;
	int extend_size = 0;
	int extend_no = 0;

	extend_size = vbfs_params.extend_size_kb * 1024;

	if ((extend = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}

	extend_no = vbfs_superblk.extend_bitmap_offset
			+ vbfs_superblk.extend_bitmap_count
			+ vbfs_params.inode_extend_cnt;

	prepare_root_dentry(extend);

	if (write_extend(extend_no, extend)) {
		free(extend);
		return -1;
	}

	free(extend);
	return ret;
}

static int write_bad_extend()
{
	int ret = 0;
	char *extend = NULL;
	int extend_size = 0;
	int extend_no = 0;
	int i;

	extend_size = vbfs_params.extend_size_kb * 1024;

	if ((extend = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}

	extend_no = vbfs_superblk.bad_extend_offset;

	memset(extend, 0, extend_size);
	for (i = 0; i < vbfs_superblk.bad_extend_count; i ++) {
		if (write_extend(extend_no, extend)) {
			free(extend);
			return -1;
		}
		extend_no ++;
	}

	free(extend);
	return ret;
}

static int write_inode_bitmap()
{
	int ret = 0;
	char *extend = NULL;
	int extend_size = 0;
	int extend_no = 0;
	__u32 inodes_total_cnt = 0;
	__u32 inodes_cnt = 0;
	__u32 inodes_bm_capacity = 0;
	__u32 i = 0;

	extend_size = vbfs_params.extend_size_kb * 1024;

	if ((extend = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}

	extend_no = vbfs_superblk.inode_bitmap_offset;
	inodes_total_cnt = vbfs_superblk.s_inode_count;
	inodes_bm_capacity = (extend_size - INODE_BITMAP_META_SIZE) * 8;

	for (i = 0; i < vbfs_superblk.inode_bitmap_count; i ++) {
		if (inodes_total_cnt > inodes_bm_capacity * (i + 1)) {
			inodes_cnt = inodes_total_cnt - inodes_bm_capacity * (i + 1);
		} else {
			inodes_cnt = 0;
		}
		vbfs_inode_bm_prepare(i, inodes_cnt, extend);

		if (write_extend(extend_no, extend)) {
			free(extend);
			return -1;
		}
		extend_no ++;
	}

	free(extend);
	return ret;
}

static int write_extend_bitmap()
{
	int ret = 0;
	char *extend = NULL;
	int extend_size = 0;
	int extend_no = 0;
	__u32 extends_total_cnt = 0;
	__u32 extends_cnt = 0;
	__u32 extends_bm_capacity = 0;
	__u32 i = 0;

	extend_size = vbfs_params.extend_size_kb * 1024;

	if ((extend = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}

	extend_no = vbfs_superblk.extend_bitmap_offset;

	extends_total_cnt = vbfs_superblk.s_extend_count
			- vbfs_superblk.extend_bitmap_offset
			- vbfs_superblk.extend_bitmap_count
			- vbfs_params.inode_extend_cnt - 1;

	/* fix total extend count */
	//vbfs_superblock.s_extend_count = extends_total_cnt;

	extends_bm_capacity = (extend_size - EXTEND_BITMAP_META_SIZE) * 8;

	for (i = 0; i < vbfs_superblk.extend_bitmap_count; i ++) {
		if (extends_total_cnt > extends_bm_capacity * (i + 1)) {
			extends_cnt = extends_total_cnt - extends_bm_capacity * (i + 1);
		} else {
			extends_cnt = 0;
		}
		vbfs_extend_bm_prepare(i, extends_cnt, extend);
		if (write_extend(extend_no, extend)) {
			free(extend);
			return -1;
		}
		extend_no ++;
	}

	free(extend);
	return ret;
}

static void superblk_to_disk(struct vbfs_superblock_disk *super_dk,
				struct vbfs_superblock *super)
{
	super_dk->s_magic = cpu_to_le32(super->s_magic);

	super_dk->s_extend_size = cpu_to_le32(super->s_extend_size);
	super_dk->s_extend_count= cpu_to_le32(super->s_extend_count);
	super_dk->s_inode_count = cpu_to_le32(super->s_inode_count);
	super_dk->s_file_idx_len = cpu_to_le32(super->s_file_idx_len);

	super_dk->bad_count = cpu_to_le32(super->bad_count);
	super_dk->bad_extend_count = cpu_to_le32(super->bad_extend_count);
	super_dk->bad_extend_current = cpu_to_le32(super->bad_extend_current);
	super_dk->bad_extend_offset = cpu_to_le32(super->bad_extend_offset);

	super_dk->extend_bitmap_count = cpu_to_le32(super->extend_bitmap_count);
	super_dk->extend_bitmap_current = cpu_to_le32(super->extend_bitmap_current);
	super_dk->extend_bitmap_offset = cpu_to_le32(super->extend_bitmap_offset);

	super_dk->inode_bitmap_count = cpu_to_le32(super->inode_bitmap_count);
	super_dk->inode_bitmap_offset = cpu_to_le32(super->inode_bitmap_offset);
	super_dk->inode_bitmap_current = cpu_to_le32(super->inode_bitmap_current);

	super_dk->s_ctime = cpu_to_le32(super->s_ctime);
	super_dk->s_mount_time = cpu_to_le32(super->s_mount_time);
	super_dk->s_state = cpu_to_le32(super->s_state);

	memcpy(super->uuid, super_dk->uuid, sizeof(super_dk->uuid));
}

static int write_superblock()
{
	int ret = 0;
	char *extend = NULL;
	int extend_size = 0;
	int extend_no = 0;
	char *pos;
	vbfs_superblock_dk_t super_disk;

	memset(&super_disk, 0, sizeof(vbfs_superblock_dk_t));
	extend_size = vbfs_params.extend_size_kb * 1024;

	if ((extend = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}
	memset(extend, 0, extend_size);

	superblk_to_disk(&super_disk.vbfs_super, &vbfs_superblk);

	pos = extend + VBFS_SUPER_OFFSET;
	memcpy(pos, &super_disk, sizeof(vbfs_superblock_dk_t));

	/* write superblock */
	if (write_extend(extend_no, extend)) {
		free(extend);
		return -1;
	}

	/* write backup superblock */
	extend_no = vbfs_superblk.s_extend_count;
	if (write_extend(extend_no, extend)) {
		free(extend);
		return -1;
	}

	free(extend);
	return ret;
}

static int vbfs_format_device()
{
	int ret = 0;

	vbfs_prepare_superblock();

	ret = write_root_inode();
	if (ret < 0) {
		return -1;
	}

	ret = write_root_dentry();
	if (ret < 0) {
		return -1;
	}

	ret = write_bad_extend();
	if (ret < 0) {
		return -1;
	}

	ret = write_inode_bitmap();
	if (ret < 0) {
		return -1;
	}

	ret = write_extend_bitmap();
	if (ret < 0) {
		return -1;
	}

	ret = write_superblock();
	if (ret < 0) {
		return -1;
	}

	fsync(vbfs_params.fd);
	close(vbfs_params.fd);

	return 0;
}

int main(int argc, char **argv)
{
	vbfs_init_paramters();
	parse_options(argc, argv);

	if (get_device_info() < 0) {
		return -1;
	}
	if (vbfs_format_device() < 0) {
		return -1;
	}

	return 0;
}

