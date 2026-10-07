#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int force, preserve;

static int copy_file(const char *source, const char *target) {
    int input = open(source, O_RDONLY);
    if (input < 0) { perror(source); return 1; }
    struct stat st, destination;
    if (fstat(input, &st)) { perror(source); close(input); return 1; }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "bootstrap cp: %s is not a regular file\n", source);
        close(input);
        return 1;
    }
    if (!stat(target, &destination) && st.st_dev == destination.st_dev &&
        st.st_ino == destination.st_ino) {
        fprintf(stderr, "bootstrap cp: source and destination are the same file\n");
        close(input);
        return 1;
    }
    int output = open(target, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (output < 0 && force) {
        if (unlink(target) && errno != ENOENT) {
            perror(target); close(input); return 1;
        }
        output = open(target, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    }
    if (output < 0) { perror(target); close(input); return 1; }
    char buffer[4096];
    int result = 0;
    ssize_t count;
    while ((count = read(input, buffer, sizeof(buffer))) > 0) {
        ssize_t written = 0;
        while (written < count) {
            ssize_t n = write(output, buffer + written, count - written);
            if (n <= 0) { perror(target); result = 1; break; }
            written += n;
        }
        if (result) break;
    }
    if (count < 0) { perror(source); result = 1; }
    if (preserve && !result) {
        struct timespec times[2];
        times[0] = st.st_atim;
        times[1] = st.st_mtim;
        if (fstat(output, &destination)) {
            perror("bootstrap cp fstat"); result = 1;
        }
        if (!result && (destination.st_uid != st.st_uid ||
                        destination.st_gid != st.st_gid) &&
            fchown(output, st.st_uid, st.st_gid)) {
            perror("bootstrap cp fchown"); result = 1;
        }
        if (!result && (destination.st_mode & 07777) != (st.st_mode & 07777) &&
            fchmod(output, st.st_mode & 07777)) {
            perror("bootstrap cp fchmod"); result = 1;
        }
        if (!result && futimens(output, times)) {
            perror("bootstrap cp futimens"); result = 1;
        }
    }
    if (close(input)) { perror(source); result = 1; }
    if (close(output)) { perror(target); result = 1; }
    return result;
}

int main(int argc, char **argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'f') force = 1;
            else if (*p == 'p') preserve = 1;
            else {
                fprintf(stderr, "bootstrap cp: unsupported option %s\n", argv[i]);
                return 2;
            }
        }
    }
    if (argc - i < 2) {
        fprintf(stderr, "bootstrap cp: source and destination required\n");
        return 2;
    }
    const char *destination = argv[argc - 1];
    struct stat st;
    int directory = !stat(destination, &st) && S_ISDIR(st.st_mode);
    if (argc - i > 2 && !directory) {
        fprintf(stderr, "bootstrap cp: multiple sources require a directory\n");
        return 1;
    }
    int result = 0;
    for (; i < argc - 1; i++) {
        if (!directory) {
            if (copy_file(argv[i], destination)) result = 1;
            continue;
        }
        const char *name = strrchr(argv[i], '/');
        name = name ? name + 1 : argv[i];
        size_t length = strlen(destination) + strlen(name) + 2;
        char *path = malloc(length);
        if (!path) { perror("malloc"); return 1; }
        snprintf(path, length, "%s/%s", destination, name);
        if (copy_file(argv[i], path)) result = 1;
        free(path);
    }
    return result;
}
