#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* TODO: check arguments, loop on short writes/EINTR, preserve close errors. */
int main(int argc, char **argv)
{
	char buffer[4096];
	int in, out;
	long n;
	if (argc != 3) {
		fprintf(stderr, "usage: %s SOURCE DEST\n", argv[0]);
		return 2;
	}
	in = open(argv[1], O_RDONLY, 0);
	if (in < 0) { perror(argv[1]); return 1; }
	out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (out < 0) { perror(argv[2]); close(in); return 1; }
	while ((n = read(in, buffer, sizeof(buffer))) > 0) {
		if (write(out, buffer, n) != n) { perror("write"); return 1; }
	}
	if (n < 0) { perror("read"); return 1; }
	if (close(in) < 0 || close(out) < 0) { perror("close"); return 1; }
	return 0;
}
