#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/inotify.h>

static void sig_handler(int signo)
{
	fflush(stdout);
	exit(0);
}
/* av[1]: dirpath */
int main(int ac, const char *av[])
{
	char buffer[16 << 10];
	const char *path;
	int fd, wd, n;

	path = av[1];
	if (ac < 2 || !path || path[0] == '\0') {
		fprintf(stderr, "Need argument: <location>\n");
		exit(1);
	}

	fd = inotify_init();
	if (fd < 0) {
		fprintf(stderr, "inotify_init1 failed: %m\n");
		exit(1);
	}
	wd = inotify_add_watch(fd, path, IN_CREATE);
	if (wd < 0) {
		fprintf(stderr, "inotify_add_watch failed: %m\n");
		exit(1);
	}
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
		const struct inotify_event *ev;
		char *ptr;
		int count = 0;
		for (ptr = &buffer[0]; ptr < buffer + n; ptr += sizeof(*ev) + ev->len) {
			ev = (const struct inotify_event *)ptr;
			if (ev->wd != wd) {
				fprintf(stderr, "Unexpected wd = %d, expected wd = %d\n", ev->wd, wd);
				continue;
			}
			printf("%s/%s\n", path, ev->name);
			count++;
		}
		if (count > 0)
			fflush(stdout);
	}
	close(fd);

	return 0;
}

#else

int main(int ac, const char *av[])
{
	fprintf(stderr, "No Support for non-Linux\n");
	return -1;
}
#endif
