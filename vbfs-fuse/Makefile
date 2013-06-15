CC ?= gcc
CFLAGS := -Wall -g -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LDFLAGS := -lfuse -lpthread

vbfs_SOURCE := $(wildcard *.c) $(wildcard engine/*.c)
vbfs_OBJS = $(vbfs_SOURCE:.c=.o)

all: vbfs_fuse

vbfs_fuse: $(vbfs_OBJS)
	$(CC) $(CFLAGS) -o $@ $(vbfs_OBJS) $(LDFLAGS)

clean:
	-rm -f $(FORMAT_OBJS) $(vbfs_OBJS) vbfs_fuse
