#include "postgres.h"

#include "catalog/pg_type_d.h"
#include "executor/executor.h"

#include "plpython.h"
#include "plpy_main.h"

#include "plpy_anytableobject.h"

static PyObject *PLy_anytable_iternext(PyObject *self);
static void PLy_anytable_dealloc(PyObject *arg);

static char PLy_anytable_doc[] = {
	"Wrapper around a Greenplum Anytable type"
};

static PyMethodDef PLy_anytable_methods[] = {
	{NULL, NULL, 0, NULL}
};

static PyTypeObject PLy_AnytableType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "PLyAnytable",
	.tp_basicsize = sizeof(PLyAnytableObject),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_ITER,
	.tp_doc = PLy_anytable_doc,
	.tp_iter = PyObject_SelfIter,
	.tp_iternext = PLy_anytable_iternext,
	.tp_methods = PLy_anytable_methods,
	.tp_dealloc = PLy_anytable_dealloc,
};

void
PLy_anytable_init_type(void)
{
	if (PyType_Ready(&PLy_AnytableType) < 0)
		elog(ERROR, "could not initialize PLy_AnytableType");
}

static void
PLy_anytable_dealloc(PyObject *arg) {
	PLyAnytableObject *anytable = (PLyAnytableObject*) arg;

	if (anytable->mcxt) {
		MemoryContextDelete(anytable->mcxt);
		anytable->mcxt = NULL;
	}

	arg->ob_type->tp_free(arg);
}

static PyObject *
PLy_anytable_iternext(PyObject *self) {
	PLyAnytableObject *anytable = (PLyAnytableObject*) self;
	PyObject *rettuple;

	HeapTuple heap = AnyTable_GetNextTuple(anytable->anytable);
	if (heap == NULL)
	{
		return NULL;
	}

	rettuple = PyTuple_New(anytable->noutinfo);

	for (int i = 0; i < anytable->noutinfo; i++)
	{
		bool is_null = false;
		Datum attr = GetAttributeByNum(heap->t_data, i + 1, &is_null);

		if (is_null)
		{
			PyTuple_SetItem(rettuple, i, Py_None);
			continue;
		}

		PyObject *item = PLy_input_convert(&anytable->outinfo[i], attr);
		PyTuple_SetItem(rettuple, i, item);
	}

	return rettuple;
}

PyObject *
PLyAnytable_FromAnytable(PLyDatumToOb *arg, Datum d) {
	PLyExecutionContext *exec_ctx = PLy_current_execution_context();
	PLyAnytableObject *anytable = NULL;
	TupleDesc desc;

	if ((anytable = PyObject_New(PLyAnytableObject, &PLy_AnytableType)) == NULL)
		return NULL;

	anytable->anytable = DatumGetAnyTable(d);
	/*
	 * the type convert function runs in exec_ctx->scratch_ctx, will reset every times.
	 * alloc a new MemoryContext to storage our data
	 */
	anytable->mcxt = AllocSetContextCreate(TopMemoryContext,
										 "PL/Python anytable context",
										 ALLOCSET_DEFAULT_SIZES);

	desc = AnyTable_GetTupleDesc(anytable->anytable);
	Assert(desc->tdtypeid == RECORDOID);

	anytable->noutinfo = desc->natts;
	anytable->outinfo = MemoryContextAlloc(anytable->mcxt, desc->natts * sizeof(PLyDatumToOb));

	for (int i = 0; i < anytable->noutinfo; i++) {
		PLy_input_setup_func(
			&anytable->outinfo[i],
			exec_ctx->scratch_ctx,
			desc->attrs[i].atttypid,
			desc->attrs[i].atttypmod,
			exec_ctx->curr_proc);
	}

	return (PyObject*) anytable;
}
