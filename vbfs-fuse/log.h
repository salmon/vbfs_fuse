#ifndef __LOG_H_
#define __LOG_H_

FILE *pLog;

#define log_err(log_file, fmt, args...) do { fprintf(log_file, fmt, ##args); fflush(log_file); } while(0)

#endif
