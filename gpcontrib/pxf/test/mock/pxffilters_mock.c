/*
 * Mock implementation for pxffilters.h
 *
 * Note that tests from pxffilters_test.c use their own mocked functions
 */

char *serializePxfFilterQuals(List *quals);

/*
 * Use for all unit tests except for tests of pxffilters
 */
char*
serializePxfFilterQuals(List *quals)
{
	return NULL;
}
