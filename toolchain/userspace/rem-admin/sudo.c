#define _GNU_SOURCE
#include <crypt.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Small REM workstation sudo.  Policy is intentionally limited to the
 * configured dev account (UID 1000); it authenticates using that account's
 * shadow password before elevating. */
static struct termios saved_termios;
static int tty_fd = -1;
static int termios_changed;

static void restore_terminal(void)
{
    if (termios_changed && tty_fd >= 0)
        (void)tcsetattr(tty_fd, TCSAFLUSH, &saved_termios);
    termios_changed = 0;
}

static void restore_and_exit(int sig)
{
    restore_terminal();
    _exit(128 + sig);
}

static int authenticate(uid_t uid)
{
    struct passwd *pw = getpwuid(uid);
    struct spwd *sp;
    struct termios hidden;
    char password[256];
    size_t used = 0;
    unsigned char ch;
    ssize_t n;
    int ok;

    if (!pw || !pw->pw_name)
        return 0;
    sp = getspnam(pw->pw_name);
    if (!sp || !sp->sp_pwdp || !*sp->sp_pwdp || sp->sp_pwdp[0] == '!'
        || sp->sp_pwdp[0] == '*')
        return 0;
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0 || tcgetattr(tty_fd, &saved_termios) < 0)
        return 0;
    hidden = saved_termios;
    hidden.c_lflag &= (tcflag_t)~ECHO;
    if (tcsetattr(tty_fd, TCSAFLUSH, &hidden) < 0)
        return 0;
    termios_changed = 1;
    (void)signal(SIGINT, restore_and_exit);
    (void)signal(SIGTERM, restore_and_exit);
    (void)signal(SIGHUP, restore_and_exit);
    dprintf(tty_fd, "[sudo] password for %s: ", pw->pw_name);
    while ((n = read(tty_fd, &ch, 1)) == 1 && ch != '\n' && ch != '\r') {
        if (ch == 8 || ch == 127) {
            if (used)
                --used;
        } else if (used + 1 < sizeof(password)) {
            password[used++] = (char)ch;
        }
    }
    password[used] = '\0';
    restore_terminal();
    (void)write(tty_fd, "\n", 1);
    (void)signal(SIGINT, SIG_DFL);
    (void)signal(SIGTERM, SIG_DFL);
    (void)signal(SIGHUP, SIG_DFL);
    if (n <= 0)
        return 0;
    {
        char *computed = crypt(password, sp->sp_pwdp);
        ok = computed && strcmp(computed, sp->sp_pwdp) == 0;
#ifdef REM_SUDO_DEBUG
        dprintf(tty_fd, "[sudo debug] uid=%lu user=%s shadow=%s input_len=%lu crypt_match=%d\n",
                (unsigned long)uid, pw->pw_name, sp->sp_pwdp ? "present" : "missing",
                (unsigned long)used, ok);
#endif
    }
    memset(password, 0, sizeof(password));
    return ok;
}

int main(int argc, char **argv)
{
    uid_t uid = getuid();
    int login = 0;
    int first = 1;
    char *const envp[] = {
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "HOME=/root", "USER=root", "LOGNAME=root", "SHELL=/bin/bash",
        "TERM=ansi", 0
    };

    if (argc == 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V"))) {
        puts("REM sudo 1.0 (GNU-compatible command subset)");
        return 0;
    }
    if (geteuid() != 0) {
        fprintf(stderr, "sudo: executable must be owned by root and mode 4755\n");
        return 126;
    }
    if (uid != 0 && uid != 1000) {
        fprintf(stderr, "sudo: user is not authorized\n");
        return 1;
    }
    if (uid != 0 && !authenticate(uid)) {
        fprintf(stderr, "sudo: authentication failed\n");
        return 1;
    }
    if (argc > 1 && (!strcmp(argv[1], "-i") || !strcmp(argv[1], "-l"))) {
        login = 1;
        first = 2;
    }
    if (setgid(0) < 0 || setuid(0) < 0) {
        perror("sudo: setuid");
        return 126;
    }
    if (login || argc == first) {
        char *shell_argv[] = {"-bash", 0};
        execve("/bin/bash", shell_argv, envp);
        execve("/bin/sh", (char *const[]){"-sh", 0}, envp);
        perror("sudo: exec shell");
        return 126;
    }
    execvpe(argv[first], &argv[first], envp);
    perror("sudo: exec command");
    return errno == ENOENT ? 127 : 126;
}
