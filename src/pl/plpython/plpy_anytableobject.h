/*
 * src/pl/plpython/plpy_anytableobject.h
 */

#ifndef PLPY_ANYTABLEOBJECT_H

#include "plpy_typeio.h"

#include "tablefuncapi.h"
#include "utils/palloc.h"

typedef struct PLyAnytableObject
{
	PyObject_HEAD;
	AnyTable anytable;
	MemoryContext mcxt;

	size_t noutinfo;
	PLyDatumToOb *outinfo;
} PLyAnytableObject;

extern void PLy_anytable_init_type(void);
extern PyObject *PLyAnytable_FromAnytable(PLyDatumToOb *arg, Datum d);

#endif	/* PLPY_ANYTABLEOBJECT_H */
