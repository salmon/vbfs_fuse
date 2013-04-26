#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "vbfs_format.h"

static struct vbfs_superblock vbfs_super;

static int dump_superblock(int fd)
{
	char buf[4096];
	int i;
	long timep;

	memset(buf, 0, sizeof(buf));

	if (lseek64(fd, VBFS_SUPER_OFFSET, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		exit(1);
	}

	if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
		fprintf(stderr, "read error\n");
		exit(1);
	}

	memcpy(&vbfs_super, buf, sizeof(vbfs_super));

	if (VBFS_SUPER_MAGIC == le32_to_cpu(vbfs_super.s_magic)) {
		printf("magic %u\n", le32_to_cpu(vbfs_super.s_magic));
	} else {
		printf("Not a vbfs filesystem\n");
		exit(1);
	}
	printf("extend size %u Bytes\n", le32_to_cpu(vbfs_super.s_extend_size));
	printf("extend count %u\n", le32_to_cpu(vbfs_super.s_extend_count));
	printf("inode count %u\n", le32_to_cpu(vbfs_super.s_inode_count));

	printf("bad extend count %u\n", le32_to_cpu(vbfs_super.bad_count));
	printf("bad max extend count %u\n", le32_to_cpu(vbfs_super.bad_extend_count));
	printf("bad extend current region %u\n", le32_to_cpu(vbfs_super.bad_extend_current));
	printf("bad extend region offset %u\n", le32_to_cpu(vbfs_super.bad_extend_offset));

	printf("extend bitmap used %u extends\n", le32_to_cpu(vbfs_super.extend_bitmap_count));
	printf("extend bitmap head at %u extends\n", le32_to_cpu(vbfs_super.extend_bitmap_offset));
	printf("extend bitmap current at %u\n", le32_to_cpu(vbfs_super.extend_bitmap_current));

	printf("inode bitmap used %u extends\n", le32_to_cpu(vbfs_super.inode_bitmap_count));
	printf("inode bitmap head at %u extends\n", le32_to_cpu(vbfs_super.inode_bitmap_offset));
	printf("inode bitmap current at %u\n", le32_to_cpu(vbfs_super.inode_bitmap_current));

	timep = le32_to_cpu(vbfs_super.s_ctime);
	printf("vbfs create at %s", ctime(&timep));
	timep = le32_to_cpu(vbfs_super.s_mount_time);
	if (le32_to_cpu(vbfs_super.s_mount_time) == 0) {
		printf("no mount time\n");
	} else {
		printf("last mount time is %s", ctime(&timep));
	}
	if (0 == le32_to_cpu(vbfs_super.s_state)) {
		printf("vbfs is clean\n"); 
	}
	else {
		printf("vbfs is dirty\n");
	}
	printf("vbfs uuid is ");
	for (i = 0; i < 16; i ++) {
		printf("%02x ", vbfs_super.uuid[i]);
	}
	printf("\n");

	return 0;
}

static int dump_inode_bitmap(int fd)
{
	off64_t offset;
	int i, l, j, tmp = 0, m = 0;
	__u32 tmp2;
	__u32 inode_bitmap_off_t;
	__u32 extend_size;
	__u32 inode_bitmap_count;
	struct inode_bitmap_group inode_metadata;
	char *buf;
	char *pos;

	extend_size = le32_to_cpu(vbfs_super.s_extend_size);
	inode_bitmap_count = le32_to_cpu(vbfs_super.inode_bitmap_count);
	inode_bitmap_off_t = le32_to_cpu(vbfs_super.inode_bitmap_offset);

	buf = malloc(extend_size);
	memset(buf, 0, extend_size);
	if (NULL == buf) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}

	for (i = 0; i < inode_bitmap_count; i ++) {
		offset = ((__u64) inode_bitmap_off_t + i) * extend_size;
		if (lseek(fd, offset, SEEK_SET) < 0) {
			fprintf(stderr, "lseek error\n");
			exit(1);
		}

		if (read(fd, buf, extend_size) != extend_size) {
			fprintf(stderr, "read error\n");
			exit(1);
		}
		memcpy(&inode_metadata, buf, sizeof(struct inode_bitmap_group));

		printf("\n###### inode group number %u ########\n", inode_metadata.group_no);
		printf("inode bitmap total %d inode room\n", inode_metadata.total_inode);
		printf("inode bitmap %u free inode room\n", inode_metadata.free_inode);
		printf("current position %u\n", inode_metadata.current_position);
		printf("first inode offset is %llu Bytes\n", inode_metadata.inode_start_offset);

		printf("\nbitmap info\n");
		pos = buf + INODE_BITMAP_META_SIZE;
		l = 0;
		for (j = 0; j < (8 * (extend_size - INODE_BITMAP_META_SIZE)); j ++) {
			if (m == 8) {
				pos ++;
				m = 0;
			}
			if (! (*pos & (1 << m))) {
				l ++;
				if (tmp == 0) {
					tmp2 = j;
				}
				tmp = 1;
			} else if (tmp) {
				printf("bitmap unused start pos %u-%u\n", tmp2, l + tmp2 - 1);
				l = 0;
				tmp = 0;
			}
			m ++;
		}
		printf("#############################################\n");
	}

	free(buf);

	return 0;
}

static int dump_extend_bitmap(int fd)
{
	off64_t offset;
	__u32 extend_bitmap_count;
	__u32 extend_bitmap_off_t;
	__u32 extend_size;
	char *buf, *pos;
	int i, l, j, tmp = 0, m = 0;
	__u32 tmp2;
	struct extend_bitmap_group extend_metadata;

	extend_size = le32_to_cpu(vbfs_super.s_extend_size);
	extend_bitmap_count = le32_to_cpu(vbfs_super.extend_bitmap_count);
	extend_bitmap_off_t = le32_to_cpu(vbfs_super.extend_bitmap_offset);

	buf = malloc(extend_size);
	memset(buf, 0, extend_size);
	if (NULL == buf) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}

	for (i = 0; i < extend_bitmap_count; i ++) {
		offset = ((__u64) extend_bitmap_off_t + i) * extend_size;
		if (lseek(fd, offset, SEEK_SET) < 0) {
			fprintf(stderr, "lseek error\n");
			exit(1);
		}

		if (read(fd, buf, extend_size) != extend_size) {
			fprintf(stderr, "read error\n");
			exit(1);
		}
		memcpy(&extend_metadata, buf, sizeof(struct extend_bitmap_group));

		printf("\n###### extend group number %u ########\n", extend_metadata.group_no);
		printf("extend bitmap total %d extend room\n", extend_metadata.total_extend);
		printf("extend bitmap %u free extend room\n", extend_metadata.free_extend);
		printf("current position %u\n", extend_metadata.current_position);
		printf("first extend offset is %llu Bytes\n", extend_metadata.extend_start_offset);

		printf("\nbitmap info\n");
		pos = buf + EXTEND_BITMAP_META_SIZE;
		l = 0;
		for (j = 0; j < (8 * (extend_size - EXTEND_BITMAP_META_SIZE)); j ++) {
			if (m == 8) {
				pos ++;
				m = 0;
			}
			if (! (*pos & (1 << m))) {
				l ++;
				if (tmp == 0) {
					tmp2 = j;
				}
				tmp = 1;
			} else if (tmp) {
				printf("bitmap unused start pos %u-%u\n", tmp2, l + tmp2 - 1);
				l = 0;
				tmp = 0;
			}
			m ++;
		}
		printf("#############################################\n\n");
	}

	return 0;
}

static int dump_inode(int fd)
{
	off64_t offset;

	offset = 0;
	printf("dump inode Not Implement\n");

	return 0;
}

int main(int argc, char **argv)
{
	int fd;
	if (argc != 2) {
		fprintf(stderr, "no device pointed\n");
	}
	fd = open(argv[1], O_RDONLY);
	dump_superblock(fd);
	dump_inode_bitmap(fd);
	dump_extend_bitmap(fd);
	dump_inode(fd);

	return 0;
}
