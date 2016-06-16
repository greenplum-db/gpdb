#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/*
 * In some cases, we hit segumentation fault around elog(ERROR, ...).  To
 * avoid any possibilities, let it do the minimum work.
 */
#define elog_finish __elog_finish

#include "../caqlanalyze.c"

#include "catalog/pg_constraint.h"
#include "catalog/pg_proc.h"
#include "utils/fmgroids.h"

#define expect_value_or_any(func, arg) do{ \
	if (use_##arg) \
		expect_value(func, arg, _##arg); \
	else \
		expect_any(func, arg); \
}while(0)

#define CaQL(q, nkeys, keys) CaQL1(q, nkeys, keys, __FILE__, __LINE__)

/*
 * Currently the only needs for elog is for ERROR to call siglongjump().
 * If you need more information such like ErrorData, you'll have to add
 * code to set it up.
 */
void
__elog_finish(int elevel, const char *fmt, ...)
{
	if (elevel == ERROR)
		PG_RE_THROW();
}

/*
 * Stub for caql1
 */
static cq_list *
CaQL1(const char *caqlStr, int maxkeys, Datum *keys, const char* filename, int lineno)
{
	cq_list	   *pcql = palloc0(sizeof(cq_list));

	pcql->bGood = true;
	pcql->caqlStr = caqlStr;
	pcql->numkeys = 0;	/* TODO: not used? */
	pcql->maxkeys = maxkeys;
	pcql->filename = filename;
	pcql->lineno = lineno;

	if (maxkeys > 0)
	{
		int			i;

		for (i = 0; i < maxkeys; i++)
		{
			pcql->cqlKeys[i] = keys[i];
		}
	}

	return pcql;
}

/*
 * Set up expects for heap_open()
 */
static void
expect__heap_open(Oid _relationId, bool use_relationId,
				  LOCKMODE _lockmode, bool use_lockmode,
				  Relation retvalue)
{
	expect_value_or_any(heap_open, relationId);
	expect_value_or_any(heap_open, lockmode);

	will_return(heap_open, retvalue);
}

/*
 * Set up expects for ScanKeyInit()
 */
static void
expect__ScanKeyInit(ScanKey _entry, bool use_entry,
				    AttrNumber _attributeNumber, bool use_attributeNumber,
				    StrategyNumber _strategy, bool use_strategy,
				    RegProcedure _procedure, bool use_procedure,
				    Datum _argument, bool use_argument)
{
	expect_value_or_any(ScanKeyInit, entry);
	expect_value_or_any(ScanKeyInit, attributeNumber);
	expect_value_or_any(ScanKeyInit, strategy);
	expect_value_or_any(ScanKeyInit, procedure);
	expect_value_or_any(ScanKeyInit, argument);

	/* returns void */
	will_return(ScanKeyInit, 0);
}

/*
 * Set up expects for beginscan
 */
static void
expect__systable_beginscan(Relation _heapRelation, bool use_heapRelation,
						   Oid _indexId, bool use_indexId,
						   bool _indexOK, bool use_indexOK,
						   Snapshot _snapshot, bool use_snapshot,
						   int _nkeys, bool use_nkeys,
						   ScanKey _key, bool use_key,
						   SysScanDesc retvalue)
{
	expect_value_or_any(systable_beginscan, heapRelation);
	expect_value_or_any(systable_beginscan, indexId);
	expect_value_or_any(systable_beginscan, indexOK);
	expect_value_or_any(systable_beginscan, snapshot);
	expect_value_or_any(systable_beginscan, nkeys);
	expect_value_or_any(systable_beginscan, key);

	will_return(systable_beginscan, retvalue);
}

/*
 * This will use syscache.
 */
void
test__caql_switch1(void **state)
{
	const char		   *query = "SELECT * FROM pg_class WHERE oid = :1";
	struct caql_hash_cookie *hash_cookie;
	cqContext			context = {0}, *pCtx;
	Datum				keys[] = {ObjectIdGetDatum(ProcedureRelationId)};
	cq_list			   *pcql = CaQL(query, 1, keys);


	expect_any(GetSysCacheId, indexoid);
	will_return(GetSysCacheId, 123);	/* pretend there is a syscache */

	hash_cookie = cq_lookup(query, strlen(query), pcql);

	pCtx = caql_switch(hash_cookie, &context, pcql);

	assert_true(pCtx != NULL);
	assert_true(pCtx->cq_sysScan == NULL);
	assert_true(pCtx->cq_usesyscache);

	hash_cookie = cq_lookup(query, strlen(query), pcql);
}

/*
 * This will use heap scan.
 */
void
test__caql_switch2(void **state)
{
	const char		   *query = "INSERT into pg_proc";
	struct caql_hash_cookie *hash_cookie;
	cqContext			context = {0}, *pCtx;
	Datum				keys[] = {};
	cq_list			   *pcql = CaQL(query, 0, keys);
	RelationData		dummyrel;

	dummyrel.rd_id = ProcedureRelationId;

	hash_cookie = cq_lookup(query, strlen(query), pcql);

	/* setup heap_open */
	expect__heap_open(ProcedureRelationId, true,
					  RowExclusiveLock, true,
					  &dummyrel);

	pCtx = caql_switch(hash_cookie, &context, pcql);

	assert_true(pCtx != NULL);
	assert_true(pCtx->cq_sysScan == NULL);
	assert_int_equal(RelationGetRelid(pCtx->cq_heap_rel), ProcedureRelationId);
}

/*
 * This will use heap scan.
 */
void
test__caql_switch3(void **state)
{
	const char		   *query = "SELECT * FROM pg_class "
								"WHERE oid = :1 AND relname = :2";
	struct caql_hash_cookie *hash_cookie;
	cqContext			context = {0}, *pCtx;
	NameData			relname = {"pg_class"};
	Datum				keys[] = {ObjectIdGetDatum(ProcedureRelationId),
								  NameGetDatum(&relname)};
	cq_list			   *pcql = CaQL(query, 1, keys);
	RelationData		dummyrel;
	SysScanDescData		dummydesc;

	dummyrel.rd_id = RelationRelationId;

	hash_cookie = cq_lookup(query, strlen(query), pcql);

	/* setup heap_open */
	expect__heap_open(RelationRelationId, true,
					  AccessShareLock, true,
					  &dummyrel);

	/* setup ScanKeyInit */
	expect__ScanKeyInit(NULL, false,
						ObjectIdAttributeNumber, true,
						BTEqualStrategyNumber, true,
						F_OIDEQ, true,
						NULL, false);

	/* setup ScanKeyInit */
	expect__ScanKeyInit(NULL, false,
						Anum_pg_class_relname, true,
						BTEqualStrategyNumber, true,
						F_NAMEEQ, true,
						NULL, false);

	/* setup systable_beginscan */
	expect__systable_beginscan(&dummyrel, true,
							   0, false,
							   0, false,
							   SnapshotNow, true,
							   2, true,
							   NULL, false,
							   &dummydesc);

	pCtx = caql_switch(hash_cookie, &context, pcql);

	assert_true(pCtx != NULL);
	assert_true(pCtx->cq_sysScan == &dummydesc);
	assert_true(pCtx->cq_heap_rel == &dummyrel);
	assert_false(pCtx->cq_usesyscache);
}

/*
 * This tests an index scan on pg_constraint.
 */
void
test__caql_switch4(void **state)
{
	const char		   *query = "SELECT * FROM pg_constraint "
								"WHERE conrelid = :1";
	struct caql_hash_cookie *hash_cookie;
	cqContext			context = {0}, *pCtx;
	Datum				keys[] = {ObjectIdGetDatum(ProcedureRelationId)};
	cq_list			   *pcql = CaQL(query, 1, keys);
	RelationData		dummyrel;
	SysScanDescData		dummydesc;

	expect_any(GetSysCacheId, indexoid);
	will_return(GetSysCacheId, 123);	/* pretend there is a syscache */

	dummyrel.rd_id = ConstraintRelationId;

	hash_cookie = cq_lookup(query, strlen(query), pcql);

	/*
	 * Add explicit relation, and set indexOK = true and syscache = false.
	 */
	pCtx = caql_addrel(cqclr(&context), &dummyrel);
	pCtx = caql_indexOK(pCtx, true);
	pCtx = caql_syscache(pCtx, false);

	/* setup ScanKeyInit */
	expect__ScanKeyInit(NULL, false,
						Anum_pg_constraint_conrelid, true,
						BTEqualStrategyNumber, true,
						F_OIDEQ, true,
						NULL, false);

	/* setup systable_beginscan */
	expect__systable_beginscan(&dummyrel, true,
							   ConstraintRelidIndexId, true,
							   true, true,
							   SnapshotNow, true,
							   1, true,
							   NULL, false,
							   &dummydesc);

	pCtx = caql_switch(hash_cookie, pCtx, pcql);

	assert_true(pCtx != NULL);
	assert_true(pCtx->cq_sysScan == &dummydesc);
	assert_true(pCtx->cq_heap_rel == &dummyrel);
	assert_false(pCtx->cq_usesyscache);
	assert_true(pCtx->cq_useidxOK);
}

/*
 * This tests if non-equal predicates also use index scan.
 */
void
test__caql_switch5(void **state)
{
	const char		   *query = "SELECT * FROM pg_attribute "
								"WHERE attrelid = :1 and attnum > :2";
	struct caql_hash_cookie *hash_cookie;
	cqContext			context = {0}, *pCtx;
	Datum				keys[] = {ObjectIdGetDatum(ProcedureRelationId),
								  Int16GetDatum(0)};
	cq_list			   *pcql = CaQL(query, 2, keys);
	RelationData		dummyrel;
	SysScanDescData		dummydesc;

	expect_any(GetSysCacheId, indexoid);
	will_return(GetSysCacheId, 123);	/* pretend there is a syscache */

	dummyrel.rd_id = AttributeRelationId;

	hash_cookie = cq_lookup(query, strlen(query), pcql);

	/*
	 * Add explicit relation, and set indexOK = true and syscache = false.
	 */
	pCtx = caql_addrel(cqclr(&context), &dummyrel);
	pCtx = caql_indexOK(pCtx, true);
	pCtx = caql_syscache(pCtx, false);

	/* setup ScanKeyInit */
	expect__ScanKeyInit(NULL, false,
						Anum_pg_attribute_attrelid, true,
						BTEqualStrategyNumber, true,
						F_OIDEQ, true,
						NULL, false);

	/* setup ScanKeyInit */
	expect__ScanKeyInit(NULL, false,
						Anum_pg_attribute_attnum, true,
						BTGreaterStrategyNumber, true,
						F_INT2GT, true,
						NULL, false);

	/* setup systable_beginscan */
	expect__systable_beginscan(&dummyrel, true,
							   AttributeRelidNumIndexId, true,
							   true, true,
							   SnapshotNow, true,
							   2, true,
							   NULL, false,
							   &dummydesc);

	pCtx = caql_switch(hash_cookie, pCtx, pcql);

	assert_true(pCtx != NULL);
	assert_true(pCtx->cq_sysScan == &dummydesc);
	assert_true(pCtx->cq_heap_rel == &dummyrel);
	assert_false(pCtx->cq_usesyscache);
	assert_true(pCtx->cq_useidxOK);
}

/*
 * This tests DELETE
 */
void
test__caql_switch6(void **state)
{
	const char		   *query = "DELETE FROM pg_class WHERE oid = :1";
	struct caql_hash_cookie *hash_cookie;
	cqContext			context = {0}, *pCtx;
	Datum				keys[] = {ObjectIdGetDatum(ProcedureRelationId)};
	cq_list			   *pcql = CaQL(query, 1, keys);
	RelationData		dummyrel;

	expect_any(GetSysCacheId, indexoid);
	will_return(GetSysCacheId, 123);	/* pretend there is a syscache */

	dummyrel.rd_id = RelationRelationId;
	hash_cookie = cq_lookup(query, strlen(query), pcql);

	/* setup heap_open */
	expect__heap_open(RelationRelationId, true,
					  RowExclusiveLock, true,
					  &dummyrel);

	pCtx = caql_switch(hash_cookie, &context, pcql);

	assert_true(pCtx != NULL);
	assert_true(pCtx->cq_usesyscache);
	assert_true(pCtx->cq_heap_rel->rd_id == RelationRelationId);
	assert_true(pCtx->cq_useidxOK);
}

/*
 * This tests operators and orderby
 */
void
test__caql_switch7(void **state)
{
	const char		   *query = "SELECT * FROM pg_class "
								"WHERE oid < :1 AND relnatts > :2 AND "
								"relfilenode <= :3 AND relpages >= :4 "
								"ORDER BY oid, relnatts, relfilenode";
	struct caql_hash_cookie *hash_cookie;
	cqContext			context = {0}, *pCtx;
	Datum				keys[] = {ObjectIdGetDatum(10000),
								  Int16GetDatum(10),
								  ObjectIdGetDatum(10000),
								  Int32GetDatum(5)};
	cq_list			   *pcql = CaQL(query, 1, keys);
	RelationData		dummyrel;
	SysScanDescData		dummydesc;
	SnapshotData		SnapshotDirty;

	InitDirtySnapshot(SnapshotDirty);
	dummyrel.rd_id = RelationRelationId;
	hash_cookie = cq_lookup(query, strlen(query), pcql);

	pCtx = caql_snapshot(cqclr(&context), &SnapshotDirty);
	/* setup heap_open */
	expect__heap_open(RelationRelationId, true,
					  AccessShareLock, true,
					  &dummyrel);

	/* setup ScanKeyInit */
	expect__ScanKeyInit(NULL, false,
						ObjectIdAttributeNumber, true,
						BTLessStrategyNumber, true,
						F_OIDLT, true,
						NULL, false);
	expect__ScanKeyInit(NULL, false,
						Anum_pg_class_relnatts, true,
						BTGreaterStrategyNumber, true,
						F_INT2GT, true,
						NULL, false);
	expect__ScanKeyInit(NULL, false,
						Anum_pg_class_relfilenode, true,
						BTLessEqualStrategyNumber, true,
						F_OIDLE, true,
						NULL, false);
	expect__ScanKeyInit(NULL, false,
						Anum_pg_class_relpages, true,
						BTGreaterEqualStrategyNumber, true,
						F_INT4GE, true,
						NULL, false);

	/* setup systable_beginscan */
	expect__systable_beginscan(&dummyrel, true,
							   InvalidOid, false,
							   false, true,
							   &SnapshotDirty, true,
							   4, true,
							   NULL, false,
							   &dummydesc);

	pCtx = caql_switch(hash_cookie, pCtx, pcql);

	assert_true(pCtx != NULL);
	assert_true(!pCtx->cq_usesyscache);
	assert_true(pCtx->cq_heap_rel->rd_id == RelationRelationId);
	assert_true(!pCtx->cq_useidxOK);
}

static void
common__cq_lookup_fail(void **state, const char *query, Datum keys[], int nkeys)
{
	struct caql_hash_cookie *hash_cookie;
	cq_list			   *pcql = CaQL(query, nkeys, keys);
	bool				result = false;

	PG_TRY();
	{
		hash_cookie = cq_lookup(query, strlen(query), pcql);
	}
	PG_CATCH();
	{
		result = true;
	}
	PG_END_TRY();

	assert_true(result);
}

/*
 * Test for non-existent column name.
 */
void
test__cq_lookup_fail1(void **state)
{
	const char		   *query = "SELECT proname from pg_type WHERE oid = :1";
	Datum				keys[] = {ObjectIdGetDatum(10000)};
	common__cq_lookup_fail(state, query, keys, 1);
}

/*
 * Test for non-existent relation name in UPDATE
 */
void
test__cq_lookup_fail2(void **state)
{
	const char		   *query = "SELECT * FROM pg_nonexistent FOR UPDATE";
	Datum				keys[] = {};
	common__cq_lookup_fail(state, query, keys, 0);
}

/*
 * Test for non-existent relation name in INSERT
 */
void
test__cq_lookup_fail3(void **state)
{
	const char		   *query = "INSERT INTO pg_nonexistent";
	Datum				keys[] = {};
	common__cq_lookup_fail(state, query, keys, 0);
}

/*
 * Test for non-existent relation name in SELECT
 */
void
test__cq_lookup_fail4(void **state)
{
	const char		   *query = "SELECT count(*) FROM pg_nonexistent";
	Datum				keys[] = {};
	common__cq_lookup_fail(state, query, keys, 0);
}

/*
 * Test for non-existent relation name in DELETE.
 */
void
test__cq_lookup_fail5(void **state)
{
	const char		   *query = "DELETE FROM pg_nonexistent";
	Datum				keys[] = {};
	common__cq_lookup_fail(state, query, keys, 0);
}

/*
 * Test for non-existent column in predicate.
 */
void
test__cq_lookup_fail6(void **state)
{
	const char		   *query = "SELECT * FROM pg_class WHERE relnonexistent = :1";
	Datum				keys[] = {ObjectIdGetDatum(42)};
	common__cq_lookup_fail(state, query, keys, 1);
}

/*
 * Test for invalid paramter number.
 */
void
test__cq_lookup_fail7(void **state)
{
	const char		   *query = "SELECT * FROM pg_class WHERE oid = :6";
	Datum				keys[] = {ObjectIdGetDatum(42)};
	common__cq_lookup_fail(state, query, keys, 1);
}

int main(int argc, char* argv[]) {
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__caql_switch1),
			unit_test(test__caql_switch2),
			unit_test(test__caql_switch3),
			unit_test(test__caql_switch4),
			unit_test(test__caql_switch5),
			unit_test(test__caql_switch6),
			unit_test(test__caql_switch7),
			unit_test(test__cq_lookup_fail1),
			unit_test(test__cq_lookup_fail2),
			unit_test(test__cq_lookup_fail3),
			unit_test(test__cq_lookup_fail4),
			unit_test(test__cq_lookup_fail5),
			unit_test(test__cq_lookup_fail6),
			unit_test(test__cq_lookup_fail7),
	};

	MemoryContextInit();

	return run_tests(tests);
}
