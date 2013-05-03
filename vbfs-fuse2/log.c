#include <syslog.h>
#include <stdarg.h>

#include "utils.h"
#include "log.h"
#include "mempool.h"

int is_debug = 0;

int log_init(void)
{
	openlog("vbfs-fuse", 0, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOG_DEBUG));

	is_debug = 1;

	return 0;
}

static void dolog(int prio, const char *fmt, va_list ap)
{
	vsyslog(prio, fmt, ap);
}

void log_warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dolog(LOG_WARNING, fmt, ap);
	va_end(ap);
}

void log_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dolog(LOG_ERR, fmt, ap);
	va_end(ap);
}

void log_debug(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dolog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

void log_close(void)
{
	closelog();
}
