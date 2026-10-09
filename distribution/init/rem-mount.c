#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>

int main(int argc, char **argv)
{
    const char *program = strrchr(argv[0], '/');
    program = program ? program + 1 : argv[0];
    if (!strcmp(program, "umount")) {
        if (argc != 2) {
            fprintf(stderr, "usage: umount TARGET\n");
            return 2;
        }
        if (!umount(argv[1]) || errno == EINVAL || errno == ENOENT)
            return 0;
        fprintf(stderr, "umount: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    const char *type = NULL, *source = NULL, *target = NULL;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            type = argv[++i];
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            i++;
        } else if (!strcmp(argv[i], "-a")) {
            return 0;
        } else {
            fprintf(stderr, "usage: mount [-t type] source target\n");
            return 2;
        }
        i++;
    }
    if (argc - i == 1) {
        target = argv[i];
    } else if (argc - i == 2) {
        source = argv[i];
        target = argv[i + 1];
    } else {
        fprintf(stderr, "usage: mount [-t type] source target\n");
        return 2;
    }
    if (!type) {
        if (!strcmp(target, "/proc")) type = "proc";
        else if (!strcmp(target, "/sys")) type = "sysfs";
        else if (!strcmp(target, "/dev")) type = "devtmpfs";
        else if (!strcmp(target, "/dev/pts")) type = "devpts";
        else type = "ext4";
    }
    if (!source) {
        if (!strcmp(type, "proc")) source = "proc";
        else if (!strcmp(type, "sysfs")) source = "sysfs";
        else if (!strcmp(type, "devtmpfs")) source = "devtmpfs";
        else if (!strcmp(type, "devpts")) source = "devpts";
        else source = "none";
    }
    if (!mount(source, target, type, MS_NOSUID, NULL) || errno == EBUSY)
        return 0;
    fprintf(stderr, "mount: %s on %s: %s\n", type, target, strerror(errno));
    return 1;
}
