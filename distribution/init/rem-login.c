#define _GNU_SOURCE
#include <crypt.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int read_secret(char *out, size_t cap)
{
    struct termios old, hidden;
    int changed = tcgetattr(STDIN_FILENO, &old) == 0;
    if (changed) {
        hidden = old;
        hidden.c_lflag &= (tcflag_t)~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden);
    }
    fputs("Password: ", stdout);
    fflush(stdout);
    if (!fgets(out, (int)cap, stdin)) {
        if (changed)
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
        return -1;
    }
    if (changed) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
        fputs("\r\n", stdout);
    }
    out[strcspn(out, "\r\n")] = '\0';
    return 0;
}

int main(int argc, char **argv)
{
    const char *name = argc > 1 ? argv[1] : "";
    struct passwd *pw = getpwnam(name);
    struct spwd *sp = getspnam(name);
    char password[256];
    if (!pw || !sp || read_secret(password, sizeof(password))) {
        fputs("Login incorrect\r\n", stdout);
        return 1;
    }
    char *computed = crypt(password, sp->sp_pwdp);
    memset(password, 0, sizeof(password));
    if (!computed || strcmp(computed, sp->sp_pwdp)) {
        fputs("Login incorrect\r\n", stdout);
        return 1;
    }

    if (initgroups(pw->pw_name, pw->pw_gid) || setgid(pw->pw_gid) ||
        setuid(pw->pw_uid)) {
        perror("rem-login: credentials");
        return 1;
    }
    if (chdir(pw->pw_dir) && chdir("/"))
        perror("rem-login: chdir");
    setenv("HOME", pw->pw_dir, 1);
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("SHELL", pw->pw_shell, 1);
    printf("Welcome to REM Linux 0.2, %s.\r\n", pw->pw_name);
    fflush(stdout);
    execl(pw->pw_shell, pw->pw_shell, "-l", (char *)NULL);
    fprintf(stderr, "rem-login: shell: %s\n", strerror(errno));
    return 1;
}
