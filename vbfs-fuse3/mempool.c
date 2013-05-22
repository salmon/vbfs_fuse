#include "mempool.h"

void *Valloc(unsigned int size)
{
	void *p = NULL;

	if ((p = valloc(size)) != NULL) {
		return p;
	} else {
		log_err("valloc error, no memory\n");
		return NULL;
	}
}

void *Malloc(unsigned int size)
{
	void *p = NULL;

	if ((p = malloc(size)) != NULL) {
		return p;
	} else {
		log_err("malloc error, no memory\n");
		return NULL;
	}
}

void *mp_malloc(unsigned int size)
{
	return Malloc(size);
}

void *mp_valloc(unsigned int size)
{
	return Valloc(size);
}

void mp_free(void *p)
{
	free(p);
}
