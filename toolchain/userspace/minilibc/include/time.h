#ifndef MYEMU_TIME_H
#define MYEMU_TIME_H
#include <sys/stat.h>
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
int clock_gettime(int clockid, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#endif
