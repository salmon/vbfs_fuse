CC ?= gcc
CFLAGS := -Wall -g -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -fstack-protector
LDFLAGS := -lfuse -lpthread

vbfs_SOURCE := extend.c io-thread.c vbfs-fuse.c direct-io.c mempool.c super.c log.c inode.c
vbfs_OBJS = $(vbfs_SOURCE:.c=.o)

test_vbfs: $(vbfs_OBJS)
	$(CC) $(CFLAGS) -o $@ $(vbfs_OBJS) $(LDFLAGS)

all: test_mio

clean:
	-rm -f $(FORMAT_OBJS) $(vbfs_OBJS) test_mio
