/*
 * xlogdump_statement.h
 *
 * a collection of functions to build/re-produce (fake) SQL statements
 * from xlog records.
 */
#ifndef __XLOGDUMP_STATEMENT_H__
#define __XLOGDUMP_STATEMENT_H__

#include "postgres.h"
#include "access/htup.h"

/* Maximum size of a null bitmap based on max number of attributes per tuple */
#define MaxNullBitmapLen	BITMAPLEN(MaxTupleAttributeNumber)

typedef struct attrib_t {
	Oid atttypid;
	char attname[NAMEDATALEN];
	int attlen;
	char attbyval;
	char attalign;
} attrib_t;

void printInsert(xl_heap_insert *, uint32, const char *);
void printUpdate(xl_heap_update *, uint32, const char *);

#endif /* __XLOGDUMP_STATEMENT_H__ */
