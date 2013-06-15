#ifndef __LOG_H_
#define __LOG_H_

#define LOG_LINE_LEN 512

extern int is_debug;

int log_init(void);
void log_debug(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_close(void);

#define log_err(fmt, args...) \
do { \
	log_error("%s(%d) " fmt, __FUNCTION__, __LINE__, ##args); \
} while(0)

#define log_dbg(fmt, args...) \
do { \
	if (is_debug) \
		log_debug("%s(%d) " fmt, __FUNCTION__, __LINE__, ##args); \
} while(0)

#endif
