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

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

struct vbfs_paramters vbfs_params;
struct vbfs_superblock vbfs_superblk;

static void vbfs_init_paramters()
{
	vbfs_params.extend_size_kb = 0;
	vbfs_params.dev_name = NULL;
	vbfs_params.total_size = 0;
	vbfs_params.file_idx_len = 256;

	vbfs_params.bad_ratio = 2048;

	vbfs_params.fd = -1;
}

static void cmd_usage()
{
	fprintf(stderr, "Usage: mkfs_vbfs [options] device\n");
	fprintf(stderr, "[options]\n");
	fprintf(stderr, "-e assign extend size in KB\n");
	fprintf(stderr, "-b assign bad ratio of extend nums\n");
	fprintf(stderr, "\t\tdefaut 1:2048\n");
	fprintf(stderr, "-x assign file index size of first extend in KB\n");
	fprintf(stderr, "\t\tdefaut 256KB\n");
	exit(1);
}

static int is_raid5_friendly()
{
	int fd;
	char name[8], buf[32], attr[128];
	int chunk_size, raid_disks, stripe_size_kb;

	name[7] = 0;
	buf[31] = 0;

	/* Fix */
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

	fd = open(vbfs_params.dev_name, O_RDWR|O_LARGEFILE);
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
		if (ioctl(fd, BLKGETSIZE64, &vbfs_params.total_size) < 0) {
			fprintf(stderr, "Can't get disk size\n");
		}
		if (0 != is_raid5_friendly()) {
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
	static const char *option_string = "e:b:x";
	int option = 0;

	while ((option = getopt(argc, argv, option_string)) != EOF) {
		switch (option) {
			case 'e':
				vbfs_params.extend_size_kb = atoi(optarg);
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

static int vbfs_prepare_superblock()
{
	__u32 extend_size = 0;
	__u64 disk_size = 0;

	__u32 extend_count = 0;
	__u32 bad_max_count = 0;
	__u32 bad_extend_count = 0;
	__u32 bm_capacity = 0;
	__u32 bitmap_cnt = 0;

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

	/* bitmap */
	bm_capacity = (extend_size - BITMAP_META_SIZE) * 8;
	bitmap_cnt = calc_div(extend_count, bm_capacity);

	memset(&vbfs_superblk, 0, sizeof(vbfs_superblk));

	/* 
 	 * generate superblock 
 	 * */
	vbfs_superblk.s_magic = VBFS_SUPER_MAGIC;
	vbfs_superblk.s_extend_size = extend_size;
	vbfs_superblk.s_extend_count = extend_count;
	vbfs_superblk.s_file_idx_len = vbfs_params.file_idx_len * 1024;

	/* bad extend */
	vbfs_superblk.bad_count = 0;
	vbfs_superblk.bad_extend_count = bad_extend_count;
	vbfs_superblk.bad_extend_current = 0;
	/* first extend is used by superblock*/
	vbfs_superblk.bad_extend_offset = 1;

	/* bitmap */
	vbfs_superblk.bitmap_count = bitmap_cnt;
	vbfs_superblk.bitmap_offset = bad_extend_count + 1;
	vbfs_superblk.bitmap_current = 0;

	vbfs_superblk.s_ctime = time(NULL);
	vbfs_superblk.s_mount_time = 0;
	/* 0 represent clean, 1 unclean */
	vbfs_superblk.s_state = 0;

	uuid_generate(vbfs_superblk.uuid);

	return 0;
}

int write_to_disk(int fd, void *buf, __u64 offset, size_t len)
{
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error %llu, %s\n", offset, strerror(errno));
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

static void bitmap_header_to_disk(bitmap_header_dk_t *bm_header_dk, struct bitmap_header *bitmap)
{
	bm_header_dk->bitmap_dk.group_no = cpu_to_le32(bitmap->group_no);
	bm_header_dk->bitmap_dk.total_cnt = cpu_to_le32(bitmap->total_cnt);
	bm_header_dk->bitmap_dk.free_cnt = cpu_to_le32(bitmap->free_cnt);
	bm_header_dk->bitmap_dk.current_position = cpu_to_le32(bitmap->current_position);
}

static void bitmap_prepare(__u32 group_no, __u32 total_cnt, char *buf)
{
	bitmap_header_dk_t bm_header_dk;
	struct bitmap_header bm_header;
	char *bitmap_region = NULL;

	__u32 extend_size;

	extend_size = vbfs_params.extend_size_kb * 1024;

	memset(&bm_header_dk, 0, sizeof(bm_header_dk));
	memset(buf, 0, extend_size);

	bitmap_region = buf + BITMAP_META_SIZE;

	bm_header.free_cnt = total_cnt;
	bm_header.total_cnt = total_cnt;
	bm_header.group_no = group_no;
	bm_header.current_position = 0;

	if (group_no == 0) {
		bm_header.free_cnt --;
		bm_header.current_position ++;
		/* one bit for root dentry */
		set_first_bits(bitmap_region, 1);
	}

	bitmap_header_to_disk(&bm_header_dk, &bm_header);

	memcpy(buf, &bm_header_dk, sizeof(bm_header_dk));
}

static int write_bitmap()
{
	char *buf = NULL;
	int extend_size = 0;
	int extend_no = 0;
	__u32 i = 0;

	__u32 count = 0;
	__u32 one_bm_capacity = 0;
	__u32 total_cnt = 0;

	extend_size = vbfs_params.extend_size_kb * 1024;
	/* minus 1 to backup superblock */
	total_cnt = vbfs_superblk.s_extend_count
			- vbfs_superblk.bitmap_count
			- vbfs_superblk.bitmap_offset - 1;
	one_bm_capacity = (extend_size - BITMAP_META_SIZE) * 8;

	if ((buf = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}

	extend_no = vbfs_superblk.bitmap_offset;

	for (i = 0; i < vbfs_superblk.bitmap_count; i ++) {
                if (total_cnt > one_bm_capacity * i) {
                        count = total_cnt - one_bm_capacity * i;
                        if (count > one_bm_capacity)
                                count = one_bm_capacity;
                } else {
                        count = 0;
                }
		bitmap_prepare(i, count, buf);
		if (write_extend(extend_no, buf)) {
			free(buf);
			return -1;
		}
		extend_no ++;
	}

	free(buf);
	return 0;
}

static void save_dirent_header(vbfs_dir_header_dk_t *vbfs_header_dk,
				struct vbfs_dirent_header *dir_header)
{
	vbfs_header_dk->vbfs_dir_header.group_no = cpu_to_le32(dir_header->group_no);
	vbfs_header_dk->vbfs_dir_header.total_extends = cpu_to_le32(dir_header->total_extends);
	vbfs_header_dk->vbfs_dir_header.dir_self_count = cpu_to_le32(dir_header->dir_self_count);
	vbfs_header_dk->vbfs_dir_header.dir_total_count = cpu_to_le32(dir_header->dir_total_count);
	vbfs_header_dk->vbfs_dir_header.next_extend = cpu_to_le32(dir_header->next_extend);
	vbfs_header_dk->vbfs_dir_header.dir_capacity = cpu_to_le32(dir_header->dir_capacity);
	vbfs_header_dk->vbfs_dir_header.bitmap_size = cpu_to_le32(dir_header->bitmap_size);
}

static void save_dirent(struct vbfs_dirent_disk *dirent_dk, struct vbfs_dirent *dir)
{
	dirent_dk->i_ino = cpu_to_le32(dir->i_ino);
	dirent_dk->i_pino = cpu_to_le32(dir->i_pino);
	dirent_dk->i_mode = cpu_to_le32(dir->i_mode);
	dirent_dk->i_size = cpu_to_le64(dir->i_size);
	dirent_dk->i_atime = cpu_to_le32(dir->i_atime);
	dirent_dk->i_ctime = cpu_to_le32(dir->i_ctime);
	dirent_dk->i_mtime = cpu_to_le32(dir->i_mtime);
	dirent_dk->padding = cpu_to_le32(dir->padding);
	memcpy(dir->name, dirent_dk->name, NAME_LEN);
}

static void prepare_root_dentry(char *buf)
{
	int bitmap_size = 0;
	int extend_size;
	int dir_cnt = 0;
	char *pos = NULL;
	struct vbfs_dirent_header dir_header;
	struct vbfs_dirent dirent;

	extend_size = vbfs_params.extend_size_kb * 1024;
	memset(buf, 0, extend_size);
	memset(&dirent, 0, sizeof(dirent));

	/* write dir header */
	dir_cnt = (extend_size - VBFS_DIR_META_SIZE) / VBFS_DIR_SIZE;
	dir_header.bitmap_size = calc_div(dir_cnt, VBFS_DIR_SIZE * CHAR_BIT);
	dir_header.dir_capacity = dir_cnt - bitmap_size;
	dir_header.group_no = 0;
	dir_header.total_extends = 1;
	dir_header.dir_self_count = 1;
	dir_header.dir_total_count = 1;
	dir_header.next_extend = 0;
	pos = buf;
	save_dirent_header((vbfs_dir_header_dk_t *) pos, &dir_header);

	/* write dir bitmap */
	pos = buf + VBFS_DIR_META_SIZE;
	set_first_bits(pos, 1);

	/* write dir */
	dirent.i_ino = ROOT_INO;
	dirent.i_pino = ROOT_INO;
	dirent.i_mode = VBFS_FT_DIR;
	dirent.i_size = extend_size;
	dirent.i_atime = time(NULL);
	dirent.i_ctime = time(NULL);
	dirent.i_mtime = time(NULL);
	dirent.padding = 0;
	memset(dirent.name, 0, NAME_LEN);
	pos = buf + VBFS_DIR_META_SIZE + dir_header.bitmap_size * VBFS_DIR_SIZE;
	save_dirent((struct vbfs_dirent_disk *) pos, &dirent);
}

static int write_root_dentry()
{
	char *buf = NULL;
	int extend_size = 0;
	int extend_no = 0;

	extend_size = vbfs_params.extend_size_kb * 1024;

	if ((buf = valloc(extend_size)) == NULL) {
		fprintf(stderr, "No mem\n");
		return -1;
	}

	extend_no = vbfs_superblk.bitmap_offset + vbfs_superblk.bitmap_count;

	printf("root dirent extend %u\n", extend_no);

	prepare_root_dentry(buf);

	if (write_extend(extend_no, buf)) {
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

static void superblk_to_disk(struct vbfs_superblock_disk *super_dk,
				struct vbfs_superblock *super)
{
	super_dk->s_magic = cpu_to_le32(super->s_magic);

	super_dk->s_extend_size = cpu_to_le32(super->s_extend_size);
	super_dk->s_extend_count= cpu_to_le32(super->s_extend_count);
	super_dk->s_file_idx_len = cpu_to_le32(super->s_file_idx_len);

	super_dk->bad_count = cpu_to_le32(super->bad_count);
	super_dk->bad_extend_count = cpu_to_le32(super->bad_extend_count);
	super_dk->bad_extend_current = cpu_to_le32(super->bad_extend_current);
	super_dk->bad_extend_offset = cpu_to_le32(super->bad_extend_offset);

	super_dk->bitmap_count = cpu_to_le32(super->bitmap_count);
	super_dk->bitmap_offset = cpu_to_le32(super->bitmap_offset);
	super_dk->bitmap_current = cpu_to_le32(super->bitmap_current);

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
	extend_no = vbfs_superblk.s_extend_count - 1;
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

	ret = write_root_dentry();
	if (ret < 0)
		return -1;

	ret = write_bad_extend();
	if (ret < 0)
		return -1;

	ret = write_bitmap();
	if (ret < 0)
		return -1;

	ret = write_superblock();
	if (ret < 0)
		return -1;

	fsync(vbfs_params.fd);
	close(vbfs_params.fd);

	return 0;
}

int main(int argc, char **argv)
{
	vbfs_init_paramters();
	parse_options(argc, argv);

	if (get_device_info() < 0)
		return -1;

	if (vbfs_format_device() < 0)
		return -1;

	return 0;
}

