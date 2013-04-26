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

#include "vbfs_format.h"

struct vbfs_paramters vbfs_params;
struct vbfs_superblock vbfs_superblk;
struct vbfs_inode vbfs_root;

static void vbfs_init_paramters()
{
	vbfs_params.extend_size_kb = 0;
	vbfs_params.dev_name = NULL;
	vbfs_params.total_size = 0;
	vbfs_params.inode_ratio = 1;
	vbfs_params.bad_ratio = 1000;
	vbfs_params.fd = -1;
}

static void cmd_usage()
{
	fprintf(stderr, "Usage: mkfs_vbfs [options] device\n");
	fprintf(stderr, "[options]\n");
	fprintf(stderr, "-e assign extend size in KB\n");
	fprintf(stderr, "-i assign inode nums ratio of extend nums\n");
	fprintf(stderr, "\t\tdefaut 1:1\n");
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
			fprintf(stderr, "Can't disk size\n");
		}
		if (0 != is_raid_friendly()) {
			fprintf(stderr, "extend size is not full stripe size\n");
		}
	} else {
		fprintf(stderr, "%s is not a block device\n", vbfs_params.dev_name);
		return -1;
	}

	return 0;
}

static void parse_options(int argc, char **argv)
{
	static const char *option_string = "e:i:b";
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

	if ((optind + 1) != argc) {
		cmd_usage();
	}

	vbfs_params.dev_name = argv[optind];
}

static int vbfs_prepare_superblock()
{
	__u32 extend_count = 0;
	__u32 bad_max_count = 0;
	__u32 bad_extend_count = 0;
	__u32 inode_count = 0;
	__u32 inode_bitmap_cnt = 0;
	__u32 inode_bitmap_off_t = 0;
	__u32 extend_bitmap_cnt = 0;
	__u32 extend_bitmap_off_t = 0;

	/* extend_count */
	printf("disk size %llu, extend size %u\n", 
		vbfs_params.total_size, vbfs_params.extend_size_kb);
	extend_count = vbfs_params.total_size / 
			(vbfs_params.extend_size_kb * 1024);
	printf("extend count %u\n", extend_count);

	/* bad extend (record bad extend number)*/
	if (extend_count % vbfs_params.bad_ratio) {
		bad_max_count = extend_count / vbfs_params.bad_ratio + 1;
	} else {
		bad_max_count = extend_count / vbfs_params.bad_ratio;
	}
	if (bad_max_count % (vbfs_params.extend_size_kb * 1024 / 8)) {
		bad_extend_count = bad_max_count / (vbfs_params.extend_size_kb * 1024 / 8) + 1;
	} else {
		bad_extend_count = bad_max_count / (vbfs_params.extend_size_kb * 1024 / 8);
	}
	printf("bad use %u extends\n", bad_extend_count);

	/* inode bitmap */
	inode_bitmap_cnt = bad_extend_count + 1;
	if (extend_count % vbfs_params.inode_ratio) {
		inode_count = extend_count / vbfs_params.inode_ratio + 1;
	} else {
		inode_count = extend_count / vbfs_params.inode_ratio;
	}
	if (inode_count % ((vbfs_params.extend_size_kb 
			- INODE_BITMAP_META_SIZE) * 1024 * 8)) {
		inode_bitmap_cnt = inode_count /
			((vbfs_params.extend_size_kb
				- INODE_BITMAP_META_SIZE) * 1024 * 8) + 1;
	} else {
		inode_bitmap_cnt = inode_count /
			((vbfs_params.extend_size_kb
				- INODE_BITMAP_META_SIZE) * 1024 * 8);
	}
	inode_bitmap_off_t = 1 + bad_extend_count;
	printf("inode bitmap use %u extend, position at %u extend\n",
			inode_bitmap_cnt, inode_bitmap_off_t);

	/* extend bitmap */
	extend_bitmap_cnt = inode_bitmap_cnt + inode_bitmap_cnt;
	if ((extend_count % ((vbfs_params.extend_size_kb 
			- EXTEND_BITMAP_META_SIZE) * 1024 * 8))) {
		extend_bitmap_cnt = extend_count /
				((vbfs_params.extend_size_kb 
					- EXTEND_BITMAP_META_SIZE) * 1024 * 8) + 1;
	} else {
		extend_bitmap_cnt = extend_count /
				((vbfs_params.extend_size_kb 
					- EXTEND_BITMAP_META_SIZE) * 1024 * 8);
	}
	extend_bitmap_off_t = inode_bitmap_off_t + inode_bitmap_cnt;
	printf("extend bitmap use %u extend, position at %u extend\n", 
			extend_bitmap_cnt, extend_bitmap_off_t);

	memset(&vbfs_superblk, 0, sizeof(vbfs_superblk));

	/* 
 	 * generate superblock 
 	 * */
	vbfs_superblk.s_magic = cpu_to_le32(VBFS_SUPER_MAGIC);
	vbfs_superblk.s_extend_size = cpu_to_le32(vbfs_params.extend_size_kb * 1024);
	vbfs_superblk.s_extend_count = cpu_to_le32(extend_count);
	vbfs_superblk.s_inode_count = cpu_to_le32(inode_count);

	/* bad extend */
	vbfs_superblk.bad_count = cpu_to_le32(0);
	vbfs_superblk.bad_extend_count = cpu_to_le32(bad_extend_count);
	vbfs_superblk.bad_extend_current = cpu_to_le32(0);
	/* first extend is used by superblock*/
	vbfs_superblk.bad_extend_offset = cpu_to_le32(1);

	/* extend bitmap */
	vbfs_superblk.extend_bitmap_count = cpu_to_le32(extend_bitmap_cnt);
	vbfs_superblk.extend_bitmap_current = cpu_to_le32(0);
	vbfs_superblk.extend_bitmap_offset = cpu_to_le32(extend_bitmap_off_t);

	/* inode bitmap */
	vbfs_superblk.inode_bitmap_count = cpu_to_le32(inode_bitmap_cnt);
	vbfs_superblk.inode_bitmap_offset = cpu_to_le32(inode_bitmap_off_t);
	vbfs_superblk.inode_bitmap_current = cpu_to_le32(0);

	vbfs_superblk.s_ctime = cpu_to_le32(time(NULL));
	vbfs_superblk.s_mount_time = 0;
	/* 0 represent clean, 1 unclean */
	vbfs_superblk.s_state = 0;

	uuid_generate(vbfs_superblk.uuid);

	return 0;
}

static int vbfs_inode_meta_prepare(__u32 groupno, char *inode_meta_buf, __u32 free_inodes)
{
	struct inode_bitmap_group inode_meta;
	__u32 bitmap_size;
	__u32 total_inode;
	__u64 start_offset;

	/* last bitmap info is wrong, will fix it later */
	memset(inode_meta_buf, 0, INODE_BITMAP_META_SIZE);
	bitmap_size = vbfs_params.extend_size_kb * 1024 - INODE_BITMAP_META_SIZE;
	total_inode = bitmap_size * 8;
	inode_meta.total_inode = cpu_to_le32(total_inode);
	inode_meta.group_no = cpu_to_le32(groupno);

	start_offset = (le32_to_cpu(vbfs_superblk.extend_bitmap_offset)
			+ le32_to_cpu(vbfs_superblk.extend_bitmap_count))
			* vbfs_params.extend_size_kb * 1024
			+ (__u64)total_inode * groupno * INODE_BITMAP_META_SIZE;
	inode_meta.inode_start_offset = cpu_to_le64(start_offset);
	printf("group %d inode bitmap, first inode start at %llu Bytes\n",
				groupno, start_offset);

	inode_meta.free_inode = cpu_to_le32(free_inodes);
	inode_meta.current_position = cpu_to_le32(0);

	memcpy(inode_meta_buf, &inode_meta, sizeof(inode_meta));

	return 0;
}

static int vbfs_extend_meta_prepare(__u32 groupno, __u32 inode_extend_count,
				char *extend_meta_buf, __u32 free_extends)
{
	struct extend_bitmap_group extend_meta;
	__u32 bitmap_size;
	__u32 total_extend;
	__u64 start_offset;

	/* last bitmap info is wrong, will fix it later */
	memset(extend_meta_buf, 0, EXTEND_BITMAP_META_SIZE);
	bitmap_size = vbfs_params.extend_size_kb * 1024 - EXTEND_BITMAP_META_SIZE;
	total_extend = bitmap_size * 8;
	extend_meta.total_extend = cpu_to_le32(total_extend);
	extend_meta.group_no = cpu_to_le32(groupno);

	start_offset = (__u64)(le32_to_cpu(vbfs_superblk.extend_bitmap_offset)
			+ le32_to_cpu(vbfs_superblk.extend_bitmap_count)
			+ inode_extend_count) * vbfs_params.extend_size_kb
			* 1024 + (__u64)total_extend
			* groupno * (vbfs_params.extend_size_kb * 1024);
	extend_meta.extend_start_offset = cpu_to_le64(start_offset);
	printf("group %d extend bitmap, first extend start at %llu Bytes\n",
				groupno, start_offset);

	extend_meta.extend_start_offset = cpu_to_le64(start_offset);
	extend_meta.free_extend = cpu_to_le32(free_extends);
	extend_meta.current_position = cpu_to_le32(0);
	memcpy(extend_meta_buf, &extend_meta, sizeof(extend_meta));

	return 0;
}

static int prepare_root_inode()
{
	memset(&vbfs_root, 0, sizeof(vbfs_root));
	vbfs_root.i_ino = 0;
	vbfs_root.i_pino = 0;
	vbfs_root.i_mode = cpu_to_le32(VBFS_FT_DIR);
	vbfs_root.i_size = 0;
	vbfs_root.i_atime = cpu_to_le32(time(NULL));
	vbfs_root.i_ctime = cpu_to_le32(time(NULL));
	vbfs_root.i_mtime = cpu_to_le32(time(NULL));

	return 0;
}

static int write_wapper(int fd, char *buf, size_t extend_size)
{
	ssize_t len;
	
	len = write(fd, buf, extend_size);
	if (len < 0) {
		fprintf(stderr, "write disk error\n");
		return -1;
	}

	return 0;
}

static __u32 alloc_extend(int *ret, __u64 *extend_offset)
{
	struct extend_bitmap_group extend_meta;
	off64_t offset;
	__u32 extend_no;
	char *buf = NULL;
	char *pos;
	int fd, extend_size;
	int i, m = 0, n = 0;
	__u32 pos_off_t;

	/* fill current extend metadata */
	fd = vbfs_params.fd;
	extend_size = vbfs_params.extend_size_kb * 1024;
	buf = malloc(extend_size);
	if (NULL == buf) {
		fprintf(stderr, "malloc error\n");
		goto err;
	}
	offset = (__u64)(le32_to_cpu(vbfs_superblk.extend_bitmap_current)
		 + le32_to_cpu(vbfs_superblk.extend_bitmap_offset)) * extend_size;
	//printf("offset1 %llu\n", offset);
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		goto err;
	}
	if (read(fd, buf, extend_size) != extend_size) {
		fprintf(stderr, "read error\n");
		goto err;
	}

	memcpy(&extend_meta, buf, sizeof(struct extend_bitmap_group));

	/* find a free extend and update metadata */
	pos_off_t = le32_to_cpu(extend_meta.current_position) / 8 + EXTEND_BITMAP_META_SIZE;
	m = le32_to_cpu(extend_meta.current_position) % 8;
	n = extend_size - pos_off_t;
	pos = buf + pos_off_t;
	for (i = 0; i < n; i ++) {
		if (m == 8) {
			pos ++;
			m = 0;
		}
		if (! (*pos & (1 << m))) {
			*pos = *pos | (1 << m);
			extend_no = le32_to_cpu(extend_meta.current_position);		
			extend_meta.current_position = cpu_to_le32(
					le32_to_cpu(extend_meta.current_position) + i + 1);
			extend_meta.free_extend = cpu_to_le32(
					 le32_to_cpu(extend_meta.free_extend) - 1);
			break;
		}
		m ++;
	}
	*extend_offset = le64_to_cpu(extend_meta.extend_start_offset) + (__u64)extend_no * extend_size;

	/* write bitmap */
	memcpy(buf, &extend_meta, sizeof(struct extend_bitmap_group));
	//printf("offset2 %llu\n", offset);
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		goto err;
	}
	if (write_wapper(fd, buf, extend_size))
		goto err;

	*ret = 0;

	return extend_no;

err:

	if (NULL != buf) {
		free(buf);
		buf = NULL;
	}
	*ret = -1;

	return -1;
}

static int write_root_dentry(__u32 extend_no, __u64 offset)
{
	char *buf = NULL;
	int extend_size, fd;
	struct dir_metadata dir_meta;
	struct vbfs_dir_entry dot_and_dotdot;
	int len;
	char *pos;

	fd = vbfs_params.fd;
	extend_size = vbfs_params.extend_size_kb * 1024;
	buf = malloc(extend_size);
	if (NULL == buf) {
		fprintf(stderr, "malloc error\n");
		goto err;
	}
	memset(buf, 0, extend_size);

	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		goto err;
	}
	/* init directory metadata */
	dir_meta.dir_count = cpu_to_le32(2);
	dir_meta.start_count = cpu_to_le32(0);
	dir_meta.next_extend = cpu_to_le32(0);
	memcpy(buf, &dir_meta, sizeof(struct dir_metadata));

	memset(&dot_and_dotdot, 0, sizeof(dot_and_dotdot));
	dot_and_dotdot.inode = 0;
	dot_and_dotdot.file_type = VBFS_FT_DIR;
	dot_and_dotdot.name[0] = '.';
	pos = buf + DIR_META_SIZE;
	len = sizeof(struct vbfs_dir_entry);
	memcpy(pos, &dot_and_dotdot, len);

	memset(&dot_and_dotdot, 0, sizeof(dot_and_dotdot));
	dot_and_dotdot.name[0] = '.';
	dot_and_dotdot.name[1] = '.';
	pos += len;
	memcpy(pos, &dot_and_dotdot, len);

	if (write_wapper(fd, buf, extend_size)) {
		goto err;
	}

	free(buf);
	buf = NULL;

	return 0;

err:
	if (NULL != buf) {
		free(buf);
		buf = NULL;
	}

	return -1;
}

static int alloc_inode()
{
	struct inode_bitmap_group inode_meta;
	off64_t offset;
	int extend_size, fd;
	char *buf = NULL;
	char *pos;
	__u32 pos_off_t;
	__u32 i, m = 0, n = 0;
	__u32 tmp = 0;
	char inode_buf[INODE_SIZE];
	__u32 inode_off_t;
	int ret = 0;
	__u64 tmp_off_t = 0;
	__u32 extend_no;

	/* find current inode bitmap */
	fd = vbfs_params.fd;
	extend_size = vbfs_params.extend_size_kb * 1024;
	buf = malloc(extend_size);
	if (NULL == buf) {
		fprintf(stderr, "malloc error\n");
		goto err;
	}
	offset = (__u64)(le32_to_cpu(vbfs_superblk.inode_bitmap_current)
		 + le32_to_cpu(vbfs_superblk.inode_bitmap_offset)) * extend_size;
	//printf("offset %llu\n", offset);
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		goto err;
	}
	if (read(fd, buf, extend_size) != extend_size) {
		fprintf(stderr, "read error\n");
		goto err;
	}

	memcpy(&inode_meta, buf, sizeof(struct inode_bitmap_group));

	/* alloc one from bitmap */
	pos_off_t = le32_to_cpu(inode_meta.current_position) / 8 + INODE_BITMAP_META_SIZE;
	m = le32_to_cpu(inode_meta.current_position) % 8;
	n = extend_size - pos_off_t;
	pos = buf + pos_off_t;
	for (i = 0; i < n; i ++) {
		if (m == 8) {
			pos ++;
			m = 0;
		}
		if (! (*pos & (1 << m))) {
			*pos = *pos | (1 << m);
			inode_off_t = le32_to_cpu(inode_meta.current_position);
			inode_meta.current_position = cpu_to_le32(inode_off_t + 1);
			inode_meta.free_inode = cpu_to_le32(
					 le32_to_cpu(inode_meta.free_inode) - 1);
			tmp = 1;
			break;
		}
		m ++;
	}
	/* bitmap is full, can't happened */
	if (tmp != 1) {
		fprintf(stderr, "Can't happend in vbfs_format\n");
		exit(1);
	}

	/* write bitmap */
	//printf("%u %u %u\n", inode_off_t, inode_meta.current_position, inode_meta.free_inode);
	memcpy(buf, &inode_meta, sizeof(struct inode_bitmap_group));
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		goto err;
	}
	if (write_wapper(fd, buf, extend_size))
		goto err;

	/* alloc extend */
	extend_no = alloc_extend(&ret, &tmp_off_t);

	/* init inode */
	if (ret == -1) {
		goto err;
	}
	vbfs_root.i_extends = cpu_to_le32(extend_no);
	memset(inode_buf, 0, sizeof(inode_buf));
	memcpy(inode_buf, &vbfs_root, sizeof(vbfs_root));
	if (extend_no != 0 || inode_off_t != 0) {
		fprintf(stderr, "Can't happend!! exit...\n");
		exit(1);
	}

	/* write inode */
	offset = (__u64)inode_off_t * INODE_SIZE + le64_to_cpu(inode_meta.inode_start_offset);
	if (lseek64(fd, offset, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		goto err;
	}
	if (write_wapper(fd, inode_buf, sizeof(inode_buf))) {
		goto err;
	}

	/* write root dentry extend, extend_no always 0 */
	if(write_root_dentry(extend_no, tmp_off_t)) {
		goto err;
	}

	free(buf);
	buf = NULL;

	return 0;

err:
	if (NULL != buf) {
		free(buf);
		buf = NULL;
	}

	return -1;
}

static int write_root_inode()
{
	int ret;

	prepare_root_inode();
	ret = alloc_inode();

	return ret;
}

static int write_to_disk()
{
	char *buf = NULL, *inode_meta_buf = NULL, *extend_meta_buf = NULL;
	int extend_size = vbfs_params.extend_size_kb * 1024;
	ssize_t ret = 0;
	off64_t offset = 0;
	int fd, i, j;
	__u32 bad_extend_cnt, inode_bitmap_cnt, extend_bitmap_cnt, inode_extend_count;
	__u32 unused_inodes, free_extends, bitmap_capacity;
	__u32 tmp1, tmp2;
	__u8 tmp_val = 0;

	fd = vbfs_params.fd;
	buf = valloc(extend_size);
	if (NULL == buf) {
		fprintf(stderr, "valloc error\n");
		goto err;
	}

	/* write bad extend info to disk*/
	offset = vbfs_params.extend_size_kb * 1024;
	ret = lseek64(vbfs_params.fd, offset, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "seek error\n");
		goto err;
	}
	memset(buf, 0, extend_size);
	bad_extend_cnt = le32_to_cpu(vbfs_superblk.bad_extend_count);
	for (i = 0; i < bad_extend_cnt; i++) {
		if (write_wapper(fd, buf, extend_size))
			goto err;
	}

	/* write inode bitmap */
	inode_meta_buf = malloc(INODE_BITMAP_META_SIZE);
	if (NULL == inode_meta_buf) {
		fprintf(stderr, "valloc error\n");
		goto err;
	}
	inode_bitmap_cnt = le32_to_cpu(vbfs_superblk.inode_bitmap_count);
	/* calculate inode used extend */
	if (le32_to_cpu((vbfs_superblk.s_inode_count) * INODE_SIZE)
				% (vbfs_params.extend_size_kb * 1024)) {
		inode_extend_count = le32_to_cpu(vbfs_superblk.s_inode_count)
				* INODE_SIZE / (vbfs_params.extend_size_kb * 1024) + 1;
	} else {
		inode_extend_count = le32_to_cpu(vbfs_superblk.s_inode_count)
				* INODE_SIZE / (vbfs_params.extend_size_kb * 1024);
	}
	printf("inode used %u extend\n", inode_extend_count);
	/* unused inode number */
	unused_inodes = inode_extend_count * vbfs_params.extend_size_kb * 1024 / INODE_SIZE;
	bitmap_capacity = (vbfs_params.extend_size_kb * 1024 - INODE_BITMAP_META_SIZE) * 8;
	printf("%u unused_inodes, bitmap capacity %u\n", unused_inodes, bitmap_capacity);
	for (i = 0; i < inode_bitmap_cnt; i++) {
		memset(buf, 0, extend_size);
		memset(inode_meta_buf, 0, INODE_BITMAP_META_SIZE);
		if (unused_inodes < bitmap_capacity) {
			tmp1 = (bitmap_capacity - unused_inodes) % 8;
			tmp2 = (bitmap_capacity - unused_inodes) / 8;
			//printf("bitmap_capacity is %u, tmp1 is %u, tmp2 is %u\n", bitmap_capacity, tmp1, tmp2);
			if (tmp1) {
				memset(buf + extend_size - tmp2, 0xFF, tmp2);
				for (j = 0; j < tmp1; j ++) {
					tmp_val = tmp_val & (1 << (7 - j));
				}
				buf[extend_size - tmp2 - 1] = tmp_val;
			} else {
				memset(buf + extend_size - tmp2, 0xFF, tmp2);
			}
			printf("last bitmap free_inode is %u\n", unused_inodes);
			vbfs_inode_meta_prepare(i, inode_meta_buf, unused_inodes);
			unused_inodes = 0;
		} else {
			unused_inodes -= bitmap_capacity;
			vbfs_inode_meta_prepare(i, inode_meta_buf, bitmap_capacity);
		}
		memcpy(buf, inode_meta_buf, INODE_BITMAP_META_SIZE);
		if (write_wapper(fd, buf, extend_size))
			goto err;
	}

	/* write extend bitmap */
	extend_meta_buf = malloc(EXTEND_BITMAP_META_SIZE);
	if (NULL == extend_meta_buf) {
		fprintf(stderr, "valloc error\n");
		goto err;
	}
	extend_bitmap_cnt = le32_to_cpu(vbfs_superblk.extend_bitmap_count);
	/* free extend number */
	free_extends = le32_to_cpu(vbfs_superblk.s_extend_count)
				- bad_extend_cnt - inode_bitmap_cnt
				- extend_bitmap_cnt - inode_extend_count;
	bitmap_capacity = (vbfs_params.extend_size_kb * 1024 - EXTEND_BITMAP_META_SIZE) * 8;
	printf("%u free_extends\n", free_extends);
	for (i = 0; i < extend_bitmap_cnt; i++) {
		memset(buf, 0, extend_size);
		memset(extend_meta_buf, 0, EXTEND_BITMAP_META_SIZE);
		if (free_extends < bitmap_capacity) {
			tmp1 = (bitmap_capacity - free_extends) % 8;
			tmp2 = (bitmap_capacity - free_extends) / 8;
			//printf("tmp1 %u, tmp2 %u\n", tmp1, tmp2);
			if (tmp1) {
				memset(buf + extend_size - tmp2, 0xFF, tmp2);
				for (j = 0; j < tmp1; j ++) {
					tmp_val = tmp_val | (1 << (7 - j));
				}
				buf[extend_size - tmp2 - 1] = tmp_val;
				//printf("%u, tmp_val %hhx\n", tmp2, tmp_val);
			} else {
				memset(buf + extend_size - tmp2, 0xFF, tmp2);
			}
			printf("last bitmap free_extend is %u\n", free_extends);
			vbfs_extend_meta_prepare(i, inode_extend_count,
						extend_meta_buf, free_extends);
			free_extends = 0;
		} else {
			free_extends -= bitmap_capacity;
			vbfs_extend_meta_prepare(i, inode_extend_count,
						extend_meta_buf, bitmap_capacity);
		}
		memcpy(buf, extend_meta_buf, EXTEND_BITMAP_META_SIZE);
		if (write_wapper(fd, buf, extend_size))
			goto err;
	}

	/* initialize inode zone (fill zero) */
	memset(buf, 0, extend_size);
	for (i = 0; i < inode_extend_count; i++) {
		if (write_wapper(fd, buf, extend_size))
			goto err;
	}

	/* write root inode */
	ret = write_root_inode();
	if (ret < 0) {
		fprintf(stderr, "write root inode error\n");
		goto err;
	}
	offset = (__u64)(le32_to_cpu(vbfs_superblk.extend_bitmap_offset)
			+ le32_to_cpu(vbfs_superblk.extend_bitmap_count))
			* vbfs_params.extend_size_kb * 1024;
	ret = lseek64(vbfs_params.fd, offset, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "seek error\n");
		goto err;
	}

	/* fix metadata value */
	/*
	 * vbfs_superblk.s_extend_count = cpu_to_le32(le32_to_cpu(vbfs_superblk.s_extend_count)
					- );
	 */

	/* write superblock info to disk */
	memset(buf, 0, extend_size);
	memcpy(buf + VBFS_SUPER_OFFSET, &vbfs_superblk, sizeof(vbfs_superblk));
	offset = 0;
	ret = lseek64(vbfs_params.fd, offset, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "seek error\n");
		goto err;
	}
	if (write_wapper(fd, buf, extend_size))
		goto err;

	return 0;
err:
	if (NULL != extend_meta_buf) {
		free(extend_meta_buf);
		extend_meta_buf = NULL;
	} else if (NULL != inode_meta_buf) {
		free(inode_meta_buf);
		inode_meta_buf = NULL;
	} else if (NULL != buf) {
		free(buf);
		buf = NULL;
	}

	return -1;
}

static int vbfs_format_device()
{
	int ret;

	vbfs_prepare_superblock();

	ret = write_to_disk();
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

