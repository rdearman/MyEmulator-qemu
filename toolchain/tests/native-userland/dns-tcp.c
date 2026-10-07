#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int request(const struct sockaddr *address, socklen_t length,
                   const char *host) {
  int fd = socket(address->sa_family, SOCK_STREAM, 0);
  if (fd < 0) { perror("socket"); return 1; }
  struct timeval timeout = {20, 0};
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ||
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) ||
      connect(fd, address, length)) {
    perror("TCP connection");
    close(fd);
    return 1;
  }
  char message[256], response[512];
  int count = snprintf(message, sizeof(message),
                       "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host);
  int sent = 0;
  while (sent < count) {
    ssize_t n = send(fd, message + sent, count - sent, 0);
    if (n <= 0) { perror("send"); close(fd); return 1; }
    sent += n;
  }
  ssize_t n = recv(fd, response, sizeof(response), 0);
  close(fd);
  if (n < 5 || memcmp(response, "HTTP/", 5)) {
    fprintf(stderr, "No HTTP response from %s\n", host);
    return 1;
  }
  return 0;
}

int main(void) {
  struct sockaddr_in raw;
  memset(&raw, 0, sizeof(raw));
  raw.sin_family = AF_INET;
  raw.sin_port = htons(80);
  if (inet_pton(AF_INET, "1.1.1.1", &raw.sin_addr) != 1 ||
      request((struct sockaddr *)&raw, sizeof(raw), "one.one.one.one"))
    return 1;
  puts("RAW_TCP_OK");
  struct addrinfo hints, *result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  int error = getaddrinfo("bbc.co.uk", "80", &hints, &result);
  if (error) { fprintf(stderr, "DNS: %s\n", gai_strerror(error)); return 2; }
  puts("DNS_HOSTNAME_OK");
  int status = request(result->ai_addr, result->ai_addrlen, "bbc.co.uk");
  freeaddrinfo(result);
  if (status) return 3;
  puts("HOSTNAME_TCP_OK");
  error = getaddrinfo("example.com", "80", &hints, &result);
  if (error) { fprintf(stderr, "repeat DNS: %s\n", gai_strerror(error)); return 4; }
  freeaddrinfo(result);
  puts("RESOLVER_NO_CRASH_OK");
  return 0;
}
