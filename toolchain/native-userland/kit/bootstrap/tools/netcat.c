#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int write_all(int fd, const char *buffer, size_t length) {
  while (length) {
    ssize_t n = write(fd, buffer, length);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { perror("nc: write"); return 1; }
    buffer += n;
    length -= n;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 3) { fprintf(stderr, "Usage: nc HOST PORT\n"); return 2; }
  struct addrinfo hints, *addresses;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  int error = getaddrinfo(argv[1], argv[2], &hints, &addresses);
  if (error) { fprintf(stderr, "nc: %s\n", gai_strerror(error)); return 1; }
  int fd = -1;
  for (struct addrinfo *a = addresses; a; a = a->ai_next) {
    fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (fd >= 0 && !connect(fd, a->ai_addr, a->ai_addrlen)) break;
    if (fd >= 0) close(fd);
    fd = -1;
  }
  freeaddrinfo(addresses);
  if (fd < 0) { perror("nc: connect"); return 1; }
  struct pollfd fds[2] = {{STDIN_FILENO, POLLIN, 0}, {fd, POLLIN, 0}};
  char buffer[4096];
  int result = 0;
  for (;;) {
    if (poll(fds, 2, -1) < 0) {
      if (errno == EINTR) continue;
      perror("nc: poll"); result = 1; break;
    }
    if (fds[0].revents & (POLLIN | POLLHUP)) {
      ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (n < 0) { perror("nc: stdin"); result = 1; break; }
      if (!n) {
        if (shutdown(fd, SHUT_WR)) { perror("nc: shutdown"); result = 1; break; }
        fds[0].fd = -1;
      } else if (write_all(fd, buffer, n)) { result = 1; break; }
    }
    if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
      ssize_t n = read(fd, buffer, sizeof(buffer));
      if (n < 0) { perror("nc: socket"); result = 1; break; }
      if (!n) break;
      if (write_all(STDOUT_FILENO, buffer, n)) { result = 1; break; }
    }
  }
  close(fd);
  return result;
}
