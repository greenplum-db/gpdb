#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define __USE_GNU
#include <dlfcn.h>

static void* (*real_malloc)(size_t) = NULL;

void *
malloc(size_t size)
{
	void *p;
	struct stat path_buf;
	int saved_errno;
	int lstat_result;

	if (real_malloc == NULL)
		real_malloc = dlsym(RTLD_NEXT, "malloc");

	p = real_malloc(size);

	saved_errno = errno;
	lstat_result = lstat("oom_malloc", &path_buf);
	errno = saved_errno;

	if (lstat_result == 0)
		return NULL;

	return p;
}
