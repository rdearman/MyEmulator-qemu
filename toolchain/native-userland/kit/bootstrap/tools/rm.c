#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int force, recursive;

static int remove_path(const char *path) {
    struct stat st;
    if (lstat(path, &st)) {
        if (force && errno == ENOENT) return 0;
        perror(path);
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        if (!unlink(path)) return 0;
        perror(path);
        return 1;
    }
    if (!recursive) {
        fprintf(stderr, "bootstrap rm: %s is a directory\n", path);
        return 1;
    }
    DIR *dir = opendir(path);
    if (!dir) { perror(path); return 1; }
    int result = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) {
            if (errno) { perror(path); result = 1; }
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        size_t length = strlen(path) + strlen(entry->d_name) + 2;
        char *child = malloc(length);
        if (!child) { perror("malloc"); result = 1; break; }
        snprintf(child, length, "%s/%s", path, entry->d_name);
        if (remove_path(child)) result = 1;
        free(child);
    }
    if (closedir(dir)) { perror(path); result = 1; }
    if (!result && rmdir(path)) { perror(path); result = 1; }
    return result;
}

int main(int argc, char **argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'f') force = 1;
            else if (*p == 'r' || *p == 'R') recursive = 1;
            else {
                fprintf(stderr, "bootstrap rm: unsupported option %s\n", argv[i]);
                return 2;
            }
        }
    }
    if (i == argc && !force) {
        fprintf(stderr, "bootstrap rm: missing operand\n");
        return 2;
    }
    int result = 0;
    for (; i < argc; i++) {
        if (recursive && (!strcmp(argv[i], "/") || !strcmp(argv[i], ".") ||
                          !strcmp(argv[i], ".."))) {
            fprintf(stderr, "bootstrap rm: refusing %s\n", argv[i]);
            result = 1;
        } else if (remove_path(argv[i])) result = 1;
    }
    return result;
}
