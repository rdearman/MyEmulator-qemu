#ifndef MYEMU_DIRENT_H
#define MYEMU_DIRENT_H
#include <stddef.h>
struct dirent { unsigned long long d_ino; long d_off; unsigned short d_reclen; unsigned char d_type; char d_name[256]; };
typedef struct { int fd; long pos; struct dirent current; unsigned char buffer[512]; int length; int offset; } DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
#define DT_UNKNOWN 0
#define DT_REG 8
#define DT_DIR 4
#endif
