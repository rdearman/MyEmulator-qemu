#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HEADER_MAX 16384
#define IO_TIMEOUT_MS 10000

static void message(const char *s)
{
	size_t n = strlen(s);
	while (n) {
		ssize_t r = write(STDERR_FILENO, s, n);
		if (r <= 0)
			return;
		s += r;
		n -= (size_t)r;
	}
	(void)write(STDERR_FILENO, "\n", 1);
}

static int write_all(int fd, const char *p, size_t n)
{
	while (n) {
		ssize_t r = write(fd, p, n);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			return -1;
		p += r;
		n -= (size_t)r;
	}
	return 0;
}

static int append(char *dst, size_t cap, size_t *used, const char *src)
{
	size_t n = strlen(src);
	if (n >= cap - *used)
		return -1;
	memcpy(dst + *used, src, n);
	*used += n;
	dst[*used] = 0;
	return 0;
}

static char *find_header_end(char *p, size_t n)
{
	for (size_t i = 3; i < n; i++)
		if (p[i - 3] == '\r' && p[i - 2] == '\n' &&
		    p[i - 1] == '\r' && p[i] == '\n')
			return p + i + 1;
	return NULL;
}

static int parse_url(const char *url, char *host, size_t hostcap,
		     char *port, size_t portcap, char *path, size_t pathcap)
{
	if (strncmp(url, "http://", 7))
		return -1;
	const char *p = url + 7;
	const char *slash = strchr(p, '/');
	const char *end = slash ? slash : p + strlen(p);
	const char *colon = NULL;
	for (const char *q = p; q < end; q++) {
		if (*q == ':' && !colon)
			colon = q;
		if (*q == '\r' || *q == '\n' || *q == '@' || *q == '[' || *q == ']')
			return -1;
	}
	const char *hostend = colon ? colon : end;
	size_t hn = (size_t)(hostend - p);
	if (!hn || hn >= hostcap)
		return -1;
	memcpy(host, p, hn);
	host[hn] = 0;
	for (size_t i = 0; i < hn; i++)
		if (!(isalnum((unsigned char)host[i]) || host[i] == '.' || host[i] == '-'))
			return -1;
	const char *portstart = colon ? colon + 1 : "80";
	size_t pn = colon ? (size_t)(end - portstart) : 2;
	if (!pn || pn >= portcap)
		return -1;
	memcpy(port, portstart, pn);
	port[pn] = 0;
	char *ep;
	long pv = strtol(port, &ep, 10);
	if (*ep || pv < 1 || pv > 65535)
		return -1;
	const char *urlpath = slash ? slash : "/";
	if (strchr(urlpath, '\r') || strchr(urlpath, '\n') || strlen(urlpath) >= pathcap)
		return -1;
	strcpy(path, urlpath);
	return 0;
}

static int connect_http(const char *host, const char *port)
{
	struct addrinfo hints, *list, *ai;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int e = getaddrinfo(host, port, &hints, &list);
	if (e) {
		message("remfetch: host lookup failed");
		return -1;
	}
	int fd = -1;
	for (ai = list; ai; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0)
			continue;
		int flags = fcntl(fd, F_GETFL, 0);
		if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
			close(fd);
			fd = -1;
			continue;
		}
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0 && errno != EINPROGRESS) {
			close(fd);
			fd = -1;
			continue;
		}
		struct pollfd pfd = { fd, POLLOUT, 0 };
		int pr;
		do { pr = poll(&pfd, 1, IO_TIMEOUT_MS); } while (pr < 0 && errno == EINTR);
		int soerr = 0;
		socklen_t sl = sizeof soerr;
		if (pr > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) == 0 && !soerr) {
			if (fcntl(fd, F_SETFL, flags) == 0)
				break;
		}
		close(fd);
		fd = -1;
	}
	freeaddrinfo(list);
	if (fd < 0)
		message("remfetch: connection failed or timed out");
	return fd;
}

static int parse_response(char *header, size_t n, uint64_t *content_length,
			  int *has_length)
{
	char *end = header + n;
	char *lineend = memmem(header, n, "\r\n", 2);
	if (!lineend || (size_t)(lineend - header) < 12 ||
	    memcmp(header, "HTTP/1.", 7) != 0) {
		message("remfetch: malformed HTTP response");
		return -1;
	}
	char *s = memchr(header, ' ', (size_t)(lineend - header));
	if (!s || s + 3 >= lineend || !isdigit((unsigned char)s[1]) ||
	    !isdigit((unsigned char)s[2]) || !isdigit((unsigned char)s[3])) {
		message("remfetch: malformed HTTP status");
		return -1;
	}
	int status = (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
	if (status != 200) {
		message("remfetch: server did not return HTTP 200");
		return -1;
	}
	*has_length = 0;
	*content_length = 0;
	for (char *p = lineend + 2; p + 2 < end;) {
		char *e = memmem(p, (size_t)(end - p), "\r\n", 2);
		if (!e || e == p)
			break;
		if ((size_t)(e - p) >= 15 && !strncasecmp(p, "Content-Length:", 15)) {
			char *v = p + 15;
			while (v < e && isspace((unsigned char)*v)) v++;
			char *ve;
			unsigned long long z = strtoull(v, &ve, 10);
			while (ve < e && isspace((unsigned char)*ve)) ve++;
			if (v == ve || ve != e) {
				message("remfetch: invalid Content-Length");
				return -1;
			}
			*content_length = z;
			*has_length = 1;
		}
		if ((size_t)(e - p) >= 18 && !strncasecmp(p, "Transfer-Encoding:", 18) &&
		    memmem(p, (size_t)(e - p), "chunked", 7)) {
			message("remfetch: chunked responses are unsupported");
			return -1;
		}
		p = e + 2;
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		message("usage: remfetch http://host[:port]/path output-file");
		return 2;
	}
	char host[256], port[8], path[2048];
	if (parse_url(argv[1], host, sizeof host, port, sizeof port, path, sizeof path)) {
		message("remfetch: only plain HTTP URLs with IPv4/DNS hostnames are supported");
		return 2;
	}
	int fd = connect_http(host, port);
	if (fd < 0)
		return 1;
	char authority[272];
	strcpy(authority, host);
	if (strcmp(port, "80")) {
		strcat(authority, ":");
		strcat(authority, port);
	}
	char request[4096];
	size_t n = 0;
	request[0] = 0;
	if (append(request, sizeof request, &n, "GET ") ||
	    append(request, sizeof request, &n, path) ||
	    append(request, sizeof request, &n, " HTTP/1.0\r\nHost: ") ||
	    append(request, sizeof request, &n, authority) ||
	    append(request, sizeof request, &n,
		   "\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n") ||
	    write_all(fd, request, n)) {
		close(fd);
		message("remfetch: request send failed");
		return 1;
	}
	char header[HEADER_MAX], chunk[4096];
	size_t hlen = 0;
	char *body = NULL;
	int out = -1;
	uint64_t expected = 0, received = 0;
	int has_length = 0;
	for (;;) {
		struct pollfd pfd = { fd, POLLIN, 0 };
		int pr;
		do { pr = poll(&pfd, 1, IO_TIMEOUT_MS); } while (pr < 0 && errno == EINTR);
		if (pr <= 0) {
			message(pr == 0 ? "remfetch: response timed out" : "remfetch: poll failed");
			goto fail;
		}
		ssize_t nr = read(fd, chunk, sizeof chunk);
		if (nr < 0 && errno == EINTR)
			continue;
		if (nr < 0) {
			message("remfetch: response read failed");
			goto fail;
		}
		if (nr == 0)
			break;
		char *data = chunk;
		size_t dlen = (size_t)nr;
		if (out < 0) {
			if (hlen + dlen > sizeof header) {
				message("remfetch: response headers too large");
				goto fail;
			}
			memcpy(header + hlen, data, dlen);
			hlen += dlen;
			body = find_header_end(header, hlen);
			if (!body)
				continue;
			size_t header_len = (size_t)(body - header);
			if (parse_response(header, header_len, &expected, &has_length))
				goto fail;
			out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (out < 0) {
				message("remfetch: cannot create output file");
				goto fail;
			}
			data = body;
			dlen = hlen - header_len;
		}
		if (dlen) {
			if (write_all(out, data, dlen)) {
				message("remfetch: output write failed");
				goto fail;
			}
			received += dlen;
		}
	}
	close(fd);
	if (out < 0) {
		message("remfetch: response ended before HTTP headers");
		return 1;
	}
	if (fsync(out) < 0) {
		if (errno == ENOSYS) {
			message("remfetch: fsync unavailable; syncing filesystems");
			sync();
		} else {
			close(out);
			unlink(argv[2]);
			message("remfetch: output fsync failed");
			return 1;
		}
	}
	if (close(out) < 0) {
		unlink(argv[2]);
		message("remfetch: output close failed");
		return 1;
	}
	if (has_length && received != expected) {
		unlink(argv[2]);
		message("remfetch: incomplete response body");
		return 1;
	}
	message("remfetch: download complete");
	return 0;
fail:
	close(fd);
	if (out >= 0)
		close(out);
	unlink(argv[2]);
	return 1;
}
