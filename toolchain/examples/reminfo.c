#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

static void dump_file(const char *title, const char *path)
{
	char buffer[512];
	long n;
	int fd = open(path, O_RDONLY, 0);

	printf("\n%s (%s)\n", title, path);
	if (fd < 0) {
		puts("  unavailable");
		return;
	}
	while ((n = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
		buffer[n] = 0;
		fputs(buffer, stdout);
	}
	close(fd);
}

static int process_count(void)
{
	DIR *dir;
	struct dirent *entry;
	int count = 0;

	dir = opendir("/proc");
	if (!dir)
		return -1;
	while ((entry = readdir(dir))) {
		const char *p = entry->d_name;
		if (*p >= '0' && *p <= '9') {
			while (*p >= '0' && *p <= '9')
				p++;
			if (!*p)
				count++;
		}
	}
	closedir(dir);
	return count;
}

static void print_platform(void)
{
	puts("\nREM platform specification (static)\n"
	     "  CPU: 32-bit, little-endian, byte-addressed, fixed-width instructions\n"
	     "  Machine: myemulator32 (internal compatibility identifier)\n"
	     "  UART: REM serial console, /dev/ttyMY0\n"
	     "  Block device: myemulator2-disk\n"
	     "  Interrupts: IRQ1-IRQ7; documented networking uses IRQ5\n"
	     "  MMIO details: see docs/REM_NETWORKING.md and docs/devices.md\n");
}

int main(void)
{
	struct utsname name;
	struct timespec uptime;
	int processes;

	puts("REM hardware and system information");
	puts("===================================");
	print_platform();

	puts("\nREM kernel report (live)\n");
	if (uname(&name) == 0) {
		printf("  sysname: %s\n  release: %s\n  version: %s\n"
		       "  machine: %s\n",
		       name.sysname, name.release, name.version, name.machine);
	} else {
		puts("  uname: unavailable");
	}
	if (clock_gettime(CLOCK_MONOTONIC, &uptime) == 0)
		printf("  uptime: %ld.%09ld seconds\n", uptime.tv_sec, uptime.tv_nsec);
	else
		puts("  uptime: unavailable");
	printf("  process count: ");
	processes = process_count();
	if (processes < 0)
		puts("unavailable");
	else
		printf("%d\n", processes);

	dump_file("Memory and load (live)", "/proc/meminfo");
	dump_file("CPU information (live)", "/proc/cpuinfo");
	dump_file("Mounted filesystems (live)", "/proc/mounts");
	dump_file("Block devices and capacity metadata (live)", "/proc/partitions");
	puts("\nStorage capacity: statfs is not yet part of the REM syscall interface;\n"
	     "  use the mounted-device and partition data above until it is added.\n");
	dump_file("Process summary (live)", "/proc/1/status");
	puts("\nreminfo: diagnostic completed");
	return 0;
}
