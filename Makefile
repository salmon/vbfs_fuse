CC ?= gcc
FORMAT_SOURCE := vbfs_format.c
DUMPFS_SOURCE := vbfs_dumpfs.c
CFLAGS := -Wall -g -D_LARGEFILE64_SOURCE
LDFLAGS := -luuid
FORMAT_OBJS = $(FORMAT_SOURCE:.c=.o)
DUMPFS_OBJS = $(DUMPFS_SOURCE:.c=.o)

all: vbfs_format

vbfs_format: $(FORMAT_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FORMAT_OBJS)

vbfs_dump: $(DUMPFS_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(DUMPFS_OBJS)


clean:
	-rm -f $(FORMAT_OBJS) $(DUMPFS_OBJS) vbfs_format vbfs_dump
