/*
 * Validate a race condition issue in pg_mkdir_p().
 *
 * pg_mkdir_p() is used by tablespace, initdb and pg_basebackup to create a
 * directory as well as its parent directories.  The logic used to be like
 * below:
 *
 *     if (stat(path) < 0)
 *     {
 *         if (mkdir(path) < 0)
 *             retval = -1;
 *     }
 *
 * It first checks for the existence of the path, if path does not pre-exist
 * then it creates the directory.  However if two processes try to create path
 * concurrently, then one possible execution order is as below:
 *
 *     A: stat(path) returns -1, so decide to create it;
 *     B: stat(path) returns -1, so decide to create it;
 *     B: mkdir(path) returns 0, successfully created path;
 *     A: mkdir(path) returns -1, fail to create path as it already exist;
 *
 * It could be triggered easily with initdb:
 *
 *     testdir=/tmp/testdir
 *     datadir=$testdir/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z
 *
 *     rm -rf $testdir
 *     mkdir $testdir
 *
 *     # init two databases with common parent directories
 *     initdb -D $datadir/db1 >$testdir/db1.log 2>&1 &
 *     initdb -D $datadir/db2 >$testdir/db2.log 2>&1 &
 *
 *     # wait for them to finish and check for the error
 *     wait
 *     grep 'could not create directory' $testdir/*.log
 *
 * The fail rate is not 100% but should be large enough to happen in 5 tries.
 *
 * This race condition could be fixed by swapping the order of stat() and
 * mkdir(), this is also what the "mkdir -p" command does.
 *
 * In this test module we test concurrent calls to pg_mkdir_p() to ensure the
 * race condition does not happen.
 */

#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

#include "port.h"
#include "utils/memutils.h"

#define TESTDIR "/tmp/testdir_pg_mkdir_p"
#define DATADIR TESTDIR "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z"

/*
 * A struct to pass arguments to the thread and return the results.
 */
typedef struct
{
	pthread_t	tid;				/* thread id */
	char		path[MAXPGPATH];	/* the path to create */
	int			retcode;			/* return code of pg_mkdir_p() */
	int			error;				/* errno */
} Job;

static void *
job_thread(void *arg)
{
	Job		   *job = (Job *) arg;

	errno = 0;

	job->retcode = pg_mkdir_p(job->path, S_IRWXU);
	job->error = errno;

	return NULL;
}

/*
 * This function accepts in integer argument n, it will launch n concurrent
 * threads to call pg_mkdir_p() to create the same dir and check for errors
 * from them.
 */
static void
concurrent_pg_mkdir_p(int n)
{
	Job			jobs[n];
	int			failed = 0;
	int			i;

	/* Create concurrent threads to execute pg_mkdir_p() */
	for (i = 0; i < n; i++)
	{
		Job		   *job = &jobs[i];

		strncpy(job->path, DATADIR, sizeof(job->path));
		pthread_create(&job->tid, NULL, job_thread, job);
	}

	/* Check for the results */
	for (i = 0; i < n; i++)
	{
		Job		   *job = &jobs[i];

		pthread_join(job->tid, NULL);

		if (job->retcode < 0)
		{
			/*
			 * Only show the message, do not error out until we joined all
			 * the threads.
			 */
			print_error("job %d: could not create directory \"%s\": %s\n",
						i, job->path, strerror(job->error));
			failed++;
		}
	}

	assert_int_equal(failed, 0);
}

static void
test__pgmkdirp__1(void **state)
{
	concurrent_pg_mkdir_p(1);
}

static void
test__pgmkdirp__2(void **state)
{
	concurrent_pg_mkdir_p(2);
}

static void
test__pgmkdirp__4(void **state)
{
	concurrent_pg_mkdir_p(4);
}

static void
test__pgmkdirp__8(void **state)
{
	concurrent_pg_mkdir_p(8);
}

static void
test__pgmkdirp__16(void **state)
{
	concurrent_pg_mkdir_p(16);
}

static void
test__pgmkdirp__32(void **state)
{
	concurrent_pg_mkdir_p(32);
}

static void
setup(void **state)
{
	/* if the dir does not exist rmtree() would raise a warning, suppress it */
	mkdir(TESTDIR, S_IRWXU);

	rmtree(TESTDIR, true);
}

static void
teardown(void **state)
{
	rmtree(TESTDIR, true);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test__pgmkdirp__1, setup, teardown),
		unit_test_setup_teardown(test__pgmkdirp__2, setup, teardown),
		unit_test_setup_teardown(test__pgmkdirp__4, setup, teardown),
		unit_test_setup_teardown(test__pgmkdirp__8, setup, teardown),
		unit_test_setup_teardown(test__pgmkdirp__16, setup, teardown),
		unit_test_setup_teardown(test__pgmkdirp__32, setup, teardown),
	};

	MemoryContextInit();

	return run_tests(tests);
}
