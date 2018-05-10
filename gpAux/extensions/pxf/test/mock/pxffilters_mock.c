/* mock implementation for pxffilters.h */

#ifndef UNIT_TESTING
PG_MODULE_MAGIC;
#endif

char *serializePxfFilterQuals(List *quals);

char* serializePxfFilterQuals(List *quals)
{
	return NULL;
}
