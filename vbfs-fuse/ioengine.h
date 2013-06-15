#ifndef __IOENGINE_H__
#define __IOENGINE_H__

#include "extend.h"

enum {
	READ = 0,
	WRITE = 1,
};

struct ioengine_ops {
	char *name;
	int (*io_init)(void);
	int (*io_exit)(void);
	int (*io_submit)(struct extend_buf *b);
};

extern struct ioengine_ops *ioengine;

void register_ioengine(struct ioengine_ops *ops);

extern void rdwr_register();

#endif
