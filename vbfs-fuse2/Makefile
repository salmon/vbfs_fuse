CC ?= gcc
CFLAGS := -Wall -g -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LDFLAGS := -lfuse

vbfs_SOURCE := fuse_vbfs.c dir.c utils.c
vbfs_OBJS = $(vbfs_SOURCE:.c=.o)

vbfs_fuse: $(vbfs_OBJS)
	$(CC) $(CFLAGS) -o $@ $(vbfs_OBJS) $(LDFLAGS)

all: vbfs_fuse

clean:
	-rm -f $(FORMAT_OBJS) $(vbfs_OBJS) vbfs_fuse
