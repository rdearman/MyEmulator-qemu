#include <dirent.h>
#include <stdio.h>

/* TODO: add sorting, filtering, and explicit opendir/readdir error handling. */
int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : ".";
	DIR *dir = opendir(path);
	struct dirent *entry;
	if (!dir) { perror(path); return 1; }
	while ((entry = readdir(dir)) != 0)
		puts(entry->d_name);
	closedir(dir);
	return 0;
}
