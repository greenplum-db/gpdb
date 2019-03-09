/*
 * Mock implementation for pxffilters.h
 *
 * Note that tests from pxffilters_test.c use their own mocked functions
 */

char *serializePxfFilterQuals(List *quals);
List	   *extractPxfAttributes(List *quals, bool *qualsAreSupported);

/*
 * Use for all unit tests except for tests of pxffilters
 */
char*
serializePxfFilterQuals(List *quals)
{
	return NULL;
}

List *
extractPxfAttributes(List *quals, bool *qualsAreSupported)
{
	check_expected(quals);
//	check_expected(qualsAreSupported);

	return (List *) mock();
}
