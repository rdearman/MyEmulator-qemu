#ifndef MYEMU_SYS_WAIT_H
#define MYEMU_SYS_WAIT_H
int waitpid(int pid, int *status, int options);
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#endif
