#include "postgres.h"

#include "access/gin_private.h"
#include <stdlib.h>

/*
 *
 * ginpostlist Fakes
 *
 */

/*
 * Ensure that assertions will trigger
 */
bool		assert_enabled = true;


void
ExceptionalCondition(const char *conditionName, const char *errorType, const char *fileName, int lineNumber)
{
	fprintf(stderr,
			"\n\nassertion failed: %s, %s, %s, line number: %d\n\n",
			conditionName,
			errorType,
			fileName,
			lineNumber);
	exit(1);
}


/*
 * Fake memory allocation methods
 */
void *
palloc(Size size)
{
	return calloc(1, size);
}


void
pfree(void *pointer)
{
	free(pointer);
}


/*
 * not used.
 */
void
tbm_add_tuples(TIDBitmap *tbm, const ItemPointer tids, int ntids, bool recheck)
{
	fprintf(stderr, "tbm_add_tuples: actually used and is not expected.");
}


void *
repalloc(void *pointer, Size size)
{
	fprintf(stderr, "repalloc: actually used and is not expected.");
	return NULL;
}

int
pg_fprintf(FILE *stream, const char *fmt,...)
{
}
