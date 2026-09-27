#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t stopped;

static void on_signal(int signo)
{
	(void)signo;
	stopped = 1;
}

int main(void)
{
	if (signal(SIGINT, on_signal) == SIG_ERR) {
		perror("signal");
		return 1;
	}
	puts("waiting; press Ctrl-C to stop");
	while (!stopped) { /* TODO: replace busy loop with supported wait. */ }
	puts("stopped cleanly");
	return 0;
}
