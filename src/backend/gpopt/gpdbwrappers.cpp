//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		gpdbwrappers.cpp
//
//	@doc:
//		Implementation of GPDB function wrappers. Note that we should never
// 		return directly from inside the PG_TRY() block, in order to restore
//		the long jump stack. That is why we save the return value of the GPDB
//		function to a local variable and return it after the PG_END_TRY().
//		./README file contains the sources (caches and catalog tables) of metadata
//		requested by the optimizer and retrieved using GPDB function wrappers. Any
//		change to optimizer's requested metadata should also be recorded in ./README file.
//
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/utils/gpdbdefs.h"

#include "gpos/base.h"
#include "gpos/error/CException.h"
#include "gpos/error/CAutoExceptionStack.h"

#include "naucrates/exception.h"

#include "gpopt/gpdbwrappers.h"

#include "utils/ext_alloc.h"

#define GP_WRAP_START	\
	sigjmp_buf local_sigjmp_buf;	\
	{	\
		CAutoExceptionStack aes((void **) &PG_exception_stack, (void**) &error_context_stack);	\
		if (0 == sigsetjmp(local_sigjmp_buf, 0))	\
		{	\
			aes.SetLocalJmp(&local_sigjmp_buf)

#define GP_WRAP_END	\
		}	\
		else \
		{ \
			GPOS_RAISE(gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError); \
		} \
	}	\

using namespace gpos;

bool
gpdb::FBoolFromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetBool(d);
	}
	GP_WRAP_END;
	return false;
}

Datum
gpdb::DDatumFromBool
	(
	bool b
	)
{
	GP_WRAP_START;
	{
		return BoolGetDatum(b);
	}
	GP_WRAP_END;
	return 0;
}

char
gpdb::CCharFromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetChar(d);
	}
	GP_WRAP_END;
	return '\0';
}

Datum
gpdb::DDatumFromChar
	(
	char c
	)
{
	GP_WRAP_START;
	{
		return CharGetDatum(c);
	}
	GP_WRAP_END;
	return 0;
}

int8
gpdb::CInt8FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetInt8(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromInt8
	(
	int8 i8
	)
{
	GP_WRAP_START;
	{
		return Int8GetDatum(i8);
	}
	GP_WRAP_END;
	return 0;
}

uint8
gpdb::UcUint8FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetUInt8(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromUint8
	(
	uint8 ui8
	)
{
	GP_WRAP_START;
	{
		return UInt8GetDatum(ui8);
	}
	GP_WRAP_END;
	return 0;
}

int16
gpdb::SInt16FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetInt16(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromInt16
	(
	int16 i16
	)
{
	GP_WRAP_START;
	{
		return Int16GetDatum(i16);
	}
	GP_WRAP_END;
	return 0;
}

uint16
gpdb::UsUint16FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetUInt16(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromUint16
	(
	uint16 ui16
	)
{
	GP_WRAP_START;
	{
		return UInt16GetDatum(ui16);
	}
	GP_WRAP_END;
	return 0;
}

int32
gpdb::IInt32FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetInt32(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromInt32
	(
	int32 i32
	)
{
	GP_WRAP_START;
	{
		return Int32GetDatum(i32);
	}
	GP_WRAP_END;
	return 0;
}

uint32
gpdb::UlUint32FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetUInt32(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromUint32
	(
	uint32 ui32
	)
{
	GP_WRAP_START;
	{
		return UInt32GetDatum(ui32);
	}
	GP_WRAP_END;
	return 0;
}

int64
gpdb::LlInt64FromDatum
	(
	Datum d
	)
{
	Datum d2 = d;
	GP_WRAP_START;
	{
		return DatumGetInt64(d2);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromInt64
	(
	int64 i64
	)
{
	int64 ii64 = i64;
	GP_WRAP_START;
	{
		return Int64GetDatum(ii64);
	}
	GP_WRAP_END;
	return 0;
}

uint64
gpdb::UllUint64FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetUInt64(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromUint64
	(
	uint64 ui64
	)
{
	GP_WRAP_START;
	{
		return UInt64GetDatum(ui64);
	}
	GP_WRAP_END;
	return 0;
}

Oid
gpdb::OidFromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetObjectId(d);
	}
	GP_WRAP_END;
	return 0;
}

void *
gpdb::PvPointerFromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetPointer(d);
	}
	GP_WRAP_END;
	return NULL;
}

float4
gpdb::FpFloat4FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetFloat4(d);
	}
	GP_WRAP_END;
	return 0;
}

float8
gpdb::DFloat8FromDatum
	(
	Datum d
	)
{
	GP_WRAP_START;
	{
		return DatumGetFloat8(d);
	}
	GP_WRAP_END;
	return 0;
}

Datum
gpdb::DDatumFromPointer
	(
	const void *p
	)
{
	GP_WRAP_START;
	{
		return PointerGetDatum(p);
	}
	GP_WRAP_END;
	return 0;
}

bool
gpdb::FAggregateExists
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		return aggregate_exists(oid);
	}
	GP_WRAP_END;
	return false;
}

Bitmapset *
gpdb::PbmsAddMember
	(
	Bitmapset *a,
	int x
	)
{
	GP_WRAP_START;
	{
		return bms_add_member(a, x);
	}
	GP_WRAP_END;
	return NULL;
}

void *
gpdb::PvCopyObject
	(
	void *from
	)
{
	GP_WRAP_START;
	{
		return copyObject(from);
	}
	GP_WRAP_END;
	return NULL;
}

Size
gpdb::SDatumSize
	(
	Datum value,
	bool typByVal,
	int iTypLen
	)
{
	GP_WRAP_START;
	{
		return datumGetSize(value, typByVal, iTypLen);
	}
	GP_WRAP_END;
	return 0;
}

void
gpdb::DeconstructArray
	(
	struct ArrayType *parray,
	Oid elmtype,
	int iElmlen,
	bool elmbyval,
	char cElmalign,
	Datum **ppElemSP,
	bool **nullsp,
	int *piElemSP
	)
{
	GP_WRAP_START;
	{
		deconstruct_array(parray, elmtype, iElmlen, elmbyval, cElmalign, ppElemSP, nullsp, piElemSP);
		return;
	}
	GP_WRAP_END;
}

Node *
gpdb::PnodeMutateExpressionTree
	(
	Node *pnode,
	Node *(*mutator) (),
	void *context
	)
{
	GP_WRAP_START;
	{
		return expression_tree_mutator(pnode, mutator, context);
	}
	GP_WRAP_END;
	return NULL;
}

bool
gpdb::FWalkExpressionTree
	(
	Node *pnode,
	bool (*walker) (),
	void *context
	)
{
	GP_WRAP_START;
	{
		return expression_tree_walker(pnode, walker, context);
	}
	GP_WRAP_END;
	return false;
}

Oid
gpdb::OidExprType
	(
	Node *pnodeExpr
	)
{
	GP_WRAP_START;
	{
		return exprType(pnodeExpr);
	}
	GP_WRAP_END;
	return 0;
}

int32
gpdb::IExprTypeMod
	(
	Node *pnodeExpr
	)
{
	GP_WRAP_START;
	{
		return exprTypmod(pnodeExpr);
	}
	GP_WRAP_END;
	return 0;
}

List *
gpdb::PlExtractNodesPlan
	(
	Plan *pl,
	int iNodeTag,
	bool descendIntoSubqueries
	)
{
	GP_WRAP_START;
	{
		return extract_nodes_plan(pl, iNodeTag, descendIntoSubqueries);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlExtractNodesExpression
	(
	Node *node,
	int iNodeTag,
	bool descendIntoSubqueries
	)
{
	GP_WRAP_START;
	{
		return extract_nodes_expression(node, iNodeTag, descendIntoSubqueries);
	}
	GP_WRAP_END;
	return NIL;
}

void
gpdb::FreeAttrStatsSlot
	(
	Oid atttype,
	Datum *pValues,
	int iValues,
	float4 *pNumbers,
	int iNumbers
	)
{
	GP_WRAP_START;
	{
		free_attstatsslot(atttype, pValues, iValues, pNumbers, iNumbers);
		return;
	}
	GP_WRAP_END;
}

bool
gpdb::FFuncStrict
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return func_strict(funcid);
	}
	GP_WRAP_END;
	return false;
}

char
gpdb::CFuncStability
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return func_volatile(funcid);
	}
	GP_WRAP_END;
	return '\0';
}

char
gpdb::CFuncDataAccess
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return func_data_access(funcid);
	}
	GP_WRAP_END;
	return '\0';
}

char
gpdb::CFuncExecLocation
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return func_exec_location(funcid);
	}
	GP_WRAP_END;
	return '\0';
}

bool
gpdb::FFunctionExists
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return function_exists(oid);
	}
	GP_WRAP_END;
	return false;
}

List *
gpdb::PlFunctionOids(void)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return function_oids();
	}
	GP_WRAP_END;
	return NIL;
}

Oid
gpdb::OidAggIntermediateResultType
	(
	Oid aggid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_aggregate */
		return get_agg_transtype(aggid);
	}
	GP_WRAP_END;
	return 0;
}

Query *
gpdb::PqueryFlattenJoinAliasVar
	(
	Query *pquery,
	gpos::ULONG ulQueryLevel
	)
{
	GP_WRAP_START;
	{
		return flatten_join_alias_var_optimizer(pquery, ulQueryLevel);
	}
	GP_WRAP_END;

	return NULL;
}

bool
gpdb::FOrderedAgg
	(
	Oid aggid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_aggregate */
		return is_agg_ordered(aggid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FAggHasPrelimFunc
	(
	Oid aggid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_aggregate */
		return has_agg_prelimfunc(aggid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FAggHasPrelimOrInvPrelimFunc
	(
	Oid aggid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_aggregate */
		return agg_has_prelim_or_invprelim_func(aggid);
	}
	GP_WRAP_END;
	return false;
}

Oid
gpdb::OidAggregate
	(
	const char *szAgg,
	Oid oidType
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_aggregate */
		return get_aggregate(szAgg, oidType);
	}
	GP_WRAP_END;
	return 0;
}

Oid
gpdb::OidArrayType
	(
	Oid typid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		return get_array_type(typid);
	}
	GP_WRAP_END;
	return 0;
}

bool
gpdb::FGetAttrStatsSlot
	(
	HeapTuple statstuple,
	Oid atttype,
	int32 atttypmod,
	int iReqKind,
	Oid reqop,
	Datum **ppValues,
	int *iValues,
	float4 **ppfNumbers,
	int *piNumbers
	)
{
	GP_WRAP_START;
	{
		return get_attstatsslot(statstuple, atttype, atttypmod, iReqKind, reqop, ppValues, iValues, ppfNumbers, piNumbers);
	}
	GP_WRAP_END;
	return false;
}

HeapTuple
gpdb::HtAttrStats
	(
	Oid relid,
	AttrNumber attnum
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_statistic */
		return get_att_stats(relid, attnum);
	}
	GP_WRAP_END;
	return NULL;
}

Oid
gpdb::OidCommutatorOp
	(
	Oid opno
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator */
		return get_commutator(opno);
	}
	GP_WRAP_END;
	return 0;
}

char *
gpdb::SzTriggerName
	(
	Oid triggerid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_trigger */
		return get_trigger_name(triggerid);
	}
	GP_WRAP_END;
	return NULL;
}

Oid
gpdb::OidTriggerRelid
	(
	Oid triggerid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_trigger */
		return get_trigger_relid(triggerid);
	}
	GP_WRAP_END;
	return 0;
}

Oid
gpdb::OidTriggerFuncid
	(
	Oid triggerid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_trigger */
		return get_trigger_funcid(triggerid);
	}
	GP_WRAP_END;
	return 0;
}

int32
gpdb::ITriggerType
	(
	Oid triggerid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_trigger */
		return get_trigger_type(triggerid);
	}
	GP_WRAP_END;
	return 0;
}

bool
gpdb::FTriggerEnabled
	(
	Oid triggerid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_trigger */
		return trigger_enabled(triggerid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FTriggerExists
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_trigger */
		return trigger_exists(oid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FCheckConstraintExists
	(
	Oid oidCheckConstraint
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_constraint */
		return check_constraint_exists(oidCheckConstraint);
	}
	GP_WRAP_END;
	return false;
}

char *
gpdb::SzCheckConstraintName
	(
	Oid oidCheckConstraint
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_constraint */
		return get_check_constraint_name(oidCheckConstraint);
	}
	GP_WRAP_END;
	return NULL;
}

Oid
gpdb::OidCheckConstraintRelid
	(
	Oid oidCheckConstraint
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_constraint */
		return get_check_constraint_relid(oidCheckConstraint);
	}
	GP_WRAP_END;
	return 0;
}

Node *
gpdb::PnodeCheckConstraint
	(
	Oid oidCheckConstraint
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_constraint */
		return get_check_constraint_expr_tree(oidCheckConstraint);
	}
	GP_WRAP_END;
	return NULL;
}

List *
gpdb::PlCheckConstraint
	(
	Oid oidRel
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_constraint */
		return get_check_constraint_oids(oidRel);
	}
	GP_WRAP_END;
	return NULL;
}

Node *
gpdb::PnodePartConstraintRel
	(
	Oid oidRel,
	List **pplDefaultLevels
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rule, pg_constraint */
		return get_relation_part_constraints(oidRel, pplDefaultLevels);
	}
	GP_WRAP_END;
	return NULL;
}

bool
gpdb::FHasExternalPartition
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rule */
		return rel_has_external_partition(oid);
	}
	GP_WRAP_END;
	return false;
}


bool
gpdb::FLeafPartition
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rule */
		return rel_is_leaf_partition(oid);
	}
	GP_WRAP_END;
	return false;
}

Oid
gpdb::OidRootPartition
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rule */
		return rel_partition_get_master(oid);
	}
	GP_WRAP_END;
	return InvalidOid;
}

bool
gpdb::FCastFunc
	(
	Oid oidSrc,
	Oid oidDest, 
	bool *is_binary_coercible,
	Oid *oidCastFunc,
	CoercionPathType *pathtype
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_cast */
		return get_cast_func(oidSrc, oidDest, is_binary_coercible, oidCastFunc, pathtype);
	}
	GP_WRAP_END;
	return false;
}

unsigned int
gpdb::UlCmpt
	(
	Oid oidOp,
	Oid oidLeft, 
	Oid oidRight
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_amop */
		return get_comparison_type(oidOp, oidLeft, oidRight);
	}
	GP_WRAP_END;
	return CmptOther;
}

Oid
gpdb::OidScCmp
	(
	Oid oidLeft, 
	Oid oidRight,
	unsigned int ulCmpt
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_amop */
		return get_comparison_operator(oidLeft, oidRight, (CmpType) ulCmpt);
	}
	GP_WRAP_END;
	return InvalidOid;
}

Oid
gpdb::OidEqualityOp
	(
	Oid oidType
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		Oid eq_opr;

		get_sort_group_operators(oidType,
					 false, true, false,
					 NULL, &eq_opr, NULL);

		return eq_opr;
	}
	GP_WRAP_END;
	return InvalidOid;
}

char *
gpdb::SzFuncName
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return get_func_name(funcid);
	}
	GP_WRAP_END;
	return NULL;
}

List *
gpdb::PlFuncOutputArgTypes
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return get_func_output_arg_types(funcid);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlFuncArgTypes
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return get_func_arg_types(funcid);
	}
	GP_WRAP_END;
	return NIL;
}

bool
gpdb::FFuncRetset
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return get_func_retset(funcid);
	}
	GP_WRAP_END;
	return false;
}

Oid
gpdb::OidFuncRetType
	(
	Oid funcid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return get_func_rettype(funcid);
	}
	GP_WRAP_END;
	return 0;
}

Oid
gpdb::OidInverseOp
	(
	Oid opno
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator */
		return get_negator(opno);
	}
	GP_WRAP_END;
	return 0;
}

RegProcedure
gpdb::OidOpFunc
	(
	Oid opno
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator */
		return get_opcode(opno);
	}
	GP_WRAP_END;
	return 0;
}

char *
gpdb::SzOpName
	(
	Oid opno
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator */
		return get_opname(opno);
	}
	GP_WRAP_END;
	return NULL;
}

List *
gpdb::PlPartitionAttrs
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		// return unique partition level attributes
		/* catalog tables: pg_partition */
		return rel_partition_keys_ordered(oid);
	}
	GP_WRAP_END;
	return NIL;
}

void
gpdb::GetOrderedPartKeysAndKinds
	(
	Oid oid,
	List **pkeys,
	List **pkinds
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition */
		rel_partition_keys_kinds_ordered(oid, pkeys, pkinds);
	}
	GP_WRAP_END;
}

PartitionNode *
gpdb::PpnParts
	(
	Oid relid,
	int2 level,
	Oid parent,
	bool inctemplate,
	bool includesubparts
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rule */
		return get_parts(relid, level, parent, inctemplate, includesubparts);
	}
	GP_WRAP_END;
	return NULL;
}

List *
gpdb::PlRelationKeys
	(
	Oid relid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_constraint */
		return get_relation_keys(relid);
	}
	GP_WRAP_END;
	return NIL;
}

Oid
gpdb::OidTypeRelid
	(
	Oid typid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		return get_typ_typrelid(typid);
	}
	GP_WRAP_END;
	return 0;
}

char *
gpdb::SzTypeName
	(
	Oid typid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		return get_type_name(typid);
	}
	GP_WRAP_END;
	return NULL;
}

int
gpdb::UlSegmentCountGP(void)
{
	GP_WRAP_START;
	{
		return getgpsegmentCount();
	}
	GP_WRAP_END;
	return 0;
}

bool
gpdb::FHeapAttIsNull
	(
	HeapTuple tup,
	int iAttNum
	)
{
	GP_WRAP_START;
	{
		return heap_attisnull(tup, iAttNum);
	}
	GP_WRAP_END;
	return false;
}

void
gpdb::FreeHeapTuple
	(
	HeapTuple htup
	)
{
	GP_WRAP_START;
	{
		heap_freetuple(htup);
		return;
	}
	GP_WRAP_END;
}

bool
gpdb::FIndexExists
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_index */
		return index_exists(oid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FGreenplumDbHashable
	(
	Oid typid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		return isGreenplumDbHashable(typid);
	}
	GP_WRAP_END;
	return false;
}

List *
gpdb::PlAppendElement
	(
	List *plist,
	void *datum
	)
{
	GP_WRAP_START;
	{
		return lappend(plist, datum);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlAppendInt
	(
	List *plist,
	int iDatum
	)
{
	GP_WRAP_START;
	{
		return lappend_int(plist, iDatum);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlAppendOid
	(
	List *plist,
	Oid datum
	)
{
	GP_WRAP_START;
	{
		return lappend_oid(plist, datum);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlPrependElement
	(
	void *datum,
	List *list
	)
{
	GP_WRAP_START;
	{
		return lcons(datum, list);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlPrependInt
	(
	int datum,
	List *list
	)
{
	GP_WRAP_START;
	{
		return lcons_int(datum, list);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlPrependOid
	(
	Oid datum,
	List *list
	)
{
	GP_WRAP_START;
	{
		return lcons_oid(datum, list);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlConcat
	(
	List *list1,
	List *list2
	)
{
	GP_WRAP_START;
	{
		return list_concat(list1, list2);
	}
	GP_WRAP_END;
	return NIL;
}

List *
gpdb::PlCopy
	(
	List *list
	)
{
	GP_WRAP_START;
	{
		return list_copy(list);
	}
	GP_WRAP_END;
	return NIL;
}

ListCell *
gpdb::PlcListHead
	(
	List *l
	)
{
	GP_WRAP_START;
	{
		return list_head(l);
	}
	GP_WRAP_END;
	return NULL;
}

ListCell *
gpdb::PlcListTail
	(
	List *l
	)
{
	GP_WRAP_START;
	{
		return list_tail(l);
	}
	GP_WRAP_END;
	return NULL;
}

uint32
gpdb::UlListLength
	(
	List *l
	)
{
	GP_WRAP_START;
	{
		return list_length(l);
	}
	GP_WRAP_END;
	return 0;
}

void *
gpdb::PvListNth
	(
	List *list,
	int n
	)
{
	GP_WRAP_START;
	{
		return list_nth(list, n);
	}
	GP_WRAP_END;
	return NULL;
}

int
gpdb::IListNth
	(
	List *list,
	int n
	)
{
	GP_WRAP_START;
	{
		return list_nth_int(list, n);
	}
	GP_WRAP_END;
	return 0;
}

Oid
gpdb::OidListNth
	(
	List *list,
	int n
	)
{
	GP_WRAP_START;
	{
		return list_nth_oid(list, n);
	}
	GP_WRAP_END;
	return 0;
}

bool
gpdb::FMemberOid
	(
	List *list,
	Oid oid
	)
{
	GP_WRAP_START;
	{
		return list_member_oid(list, oid);
	}
	GP_WRAP_END;
	return false;
}

void
gpdb::FreeList
	(
	List *plist
	)
{
	GP_WRAP_START;
	{
		list_free(plist);
		return;
	}
	GP_WRAP_END;
}

void
gpdb::FreeListDeep
	(
	List *plist
	)
{
	GP_WRAP_START;
	{
		list_free_deep(plist);
		return;
	}
	GP_WRAP_END;
}

bool
gpdb::FMotionGather
	(
	const Motion *pmotion
	)
{
	GP_WRAP_START;
	{
		return isMotionGather(pmotion);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FAppendOnlyPartitionTable
	(
	Oid rootOid
	)
{
	GP_WRAP_START;
	{
		return rel_has_appendonly_partition(rootOid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FMultilevelPartitionUniform
	(
	Oid rootOid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rule, pg_constraint */
		return rel_partitioning_is_uniform(rootOid);
	}
	GP_WRAP_END;
	return false;
}

TypeCacheEntry *
gpdb::PtceLookup
	(
	Oid type_id,
	int flags
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type, pg_operator, pg_opclass, pg_opfamily, pg_amop */
		return lookup_type_cache(type_id, flags);
	}
	GP_WRAP_END;
	return NULL;
}

Value *
gpdb::PvalMakeString
	(
	char *str
	)
{
	GP_WRAP_START;
	{
		return makeString(str);
	}
	GP_WRAP_END;
	return NULL;
}

Value *
gpdb::PvalMakeInteger
	(
	long i
	)
{
	GP_WRAP_START;
	{
		return makeInteger(i);
	}
	GP_WRAP_END;
	return NULL;
}

Node *
gpdb::PnodeMakeBoolConst
	(
	bool value,
	bool isnull
	)
{
	GP_WRAP_START;
	{
		return makeBoolConst(value, isnull);
	}
	GP_WRAP_END;
	return NULL;
}

Node *
gpdb::PnodeMakeNULLConst
	(
	Oid oidType
	)
{
	GP_WRAP_START;
	{
		return (Node *) makeNullConst(oidType, -1 /*consttypmod*/);
	}
	GP_WRAP_END;
	return NULL;
}

TargetEntry *
gpdb::PteMakeTargetEntry
	(
	Expr *pnodeExpr,
	AttrNumber resno,
	char *resname,
	bool resjunk
	)
{
	GP_WRAP_START;
	{
		return makeTargetEntry(pnodeExpr, resno, resname, resjunk);
	}
	GP_WRAP_END;
	return NULL;
}

Var *
gpdb::PvarMakeVar
	(
	Index varno,
	AttrNumber varattno,
	Oid vartype,
	int32 vartypmod,
	Index varlevelsup
	)
{
	GP_WRAP_START;
	{
		return makeVar(varno, varattno, vartype, vartypmod, varlevelsup);
	}
	GP_WRAP_END;
	return NULL;
}

void *
gpdb::PvMemoryContextAllocImpl
	(
	MemoryContext context,
	Size size,
	const char* file,
	const char * func,
	int line
	)
{
	GP_WRAP_START;
	{
		return MemoryContextAllocImpl(context, size, file, func, line);
	}
	GP_WRAP_END;
	return NULL;
}

void *
gpdb::PvMemoryContextAllocZeroAlignedImpl
	(
	MemoryContext context,
	Size size,
	const char* file,
	const char * func,
	int line
	)
{
	GP_WRAP_START;
	{
		return MemoryContextAllocZeroAlignedImpl(context, size, file, func, line);
	}
	GP_WRAP_END;
	return NULL;
}

void *
gpdb::PvMemoryContextAllocZeroImpl
	(
	MemoryContext context,
	Size size,
	const char* file,
	const char * func,
	int line
	)
{
	GP_WRAP_START;
	{
		return MemoryContextAllocZeroImpl(context, size, file, func, line);
	}
	GP_WRAP_END;
	return NULL;
}

void *
gpdb::PvMemoryContextReallocImpl
	(
	void *pointer,
	Size size,
	const char* file,
	const char * func,
	int line
	)
{
	GP_WRAP_START;
	{
		return MemoryContextReallocImpl(pointer, size, file, func, line);
	}
	GP_WRAP_END;
	return NULL;
}

char *
gpdb::SzMemoryContextStrdup
	(
	MemoryContext context,
	const char *string
	)
{
	GP_WRAP_START;
	{
		return MemoryContextStrdup(context, string);
	}
	GP_WRAP_END;
	return NULL;
}

// Helper function to throw an error with errcode, message and hint, like you
// would with ereport(...) in the backend. This could be extended for other
// fields, but this is all we need at the moment.
void
gpdb::GpdbEreportImpl
	(
	int xerrcode,
	int severitylevel,
	const char *xerrmsg,
	const char *xerrhint,
	const char *filename,
	int lineno,
	const char *funcname
	)
{
	GP_WRAP_START;
	{
		// We cannot use the ereport() macro here, because we want to pass on
		// the caller's filename and line number. This is essentially an
		// expanded version of ereport(). It will be caught by the
		// GP_WRAP_END, and propagated up as a C++ exception, to be
		// re-thrown as a Postgres error once we leave the C++ land.
		if (errstart(severitylevel, filename, lineno, funcname, TEXTDOMAIN))
			errfinish (errcode(xerrcode),
					   errmsg("%s", xerrmsg),
					   xerrhint ? errhint("%s", xerrhint) : 0);
	}
	GP_WRAP_END;
}

char *
gpdb::SzNodeToString
	(
	void *obj
	)
{
	GP_WRAP_START;
	{
		return nodeToString(obj);
	}
	GP_WRAP_END;
	return NULL;
}

Node *
gpdb::Pnode
	(
	char *string
	)
{
	GP_WRAP_START;
	{
		return (Node*) stringToNode(string);
	}
	GP_WRAP_END;
	return NULL;
}


Node *
gpdb::PnodeTypeDefault
	(
	Oid typid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		return get_typdefault(typid);
	}
	GP_WRAP_END;
	return NULL;
}


double
gpdb::DNumericToDoubleNoOverflow
	(
	Numeric num
	)
{
	GP_WRAP_START;
	{
		return numeric_to_double_no_overflow(num);
	}
	GP_WRAP_END;
	return 0.0;
}

double
gpdb::DConvertTimeValueToScalar
	(
	Datum datum,
	Oid typid
	)
{
	GP_WRAP_START;
	{
		return convert_timevalue_to_scalar(datum, typid);
	}
	GP_WRAP_END;
	return 0.0;
}

double
gpdb::DConvertNetworkToScalar
	(
	Datum datum,
	Oid typid
	)
{
	GP_WRAP_START;
	{
		return convert_network_to_scalar(datum, typid);
	}
	GP_WRAP_END;
	return 0.0;
}

bool
gpdb::FOpHashJoinable
	(
	Oid opno
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator */
		return op_hashjoinable(opno);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FOpStrict
	(
	Oid opno
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator, pg_proc */
		return op_strict(opno);
	}
	GP_WRAP_END;
	return false;
}

void
gpdb::GetOpInputTypes
	(
	Oid opno,
	Oid *lefttype,
	Oid *righttype
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator */
		op_input_types(opno, lefttype, righttype);
		return;
	}
	GP_WRAP_END;
}

bool
gpdb::FOperatorExists
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_operator */
		return operator_exists(oid);
	}
	GP_WRAP_END;
	return false;
}

void *
gpdb::GPDBAlloc
	(
	Size size
	)
{
	GP_WRAP_START;
	{
		return palloc(size);
	}
	GP_WRAP_END;
	return NULL;
}

void
gpdb::GPDBFree
	(
	void *ptr
	)
{
	GP_WRAP_START;
	{
		pfree(ptr);
		return;
	}
	GP_WRAP_END;
}

struct varlena *
gpdb::PvlenDetoastDatum
	(
	struct varlena * datum
	)
{
	GP_WRAP_START;
	{
		return pg_detoast_datum(datum);
	}
	GP_WRAP_END;
	return NULL;
}

bool
gpdb::FWalkQueryOrExpressionTree
	(
	Node *pnode,
	bool (*walker) (),
	void *context,
	int flags
	)
{
	GP_WRAP_START;
	{
		return query_or_expression_tree_walker(pnode, walker, context, flags);
	}
	GP_WRAP_END;
	return false;
}

Node *
gpdb::PnodeMutateQueryOrExpressionTree
	(
	Node *pnode,
	Node *(*mutator) (),
	void *context,
	int flags
	)
{
	GP_WRAP_START;
	{
		return query_or_expression_tree_mutator(pnode, mutator, context, flags);
	}
	GP_WRAP_END;
	return NULL;
}

Query *
gpdb::PqueryMutateQueryTree
	(
	Query *query,
	Node *(*mutator) (),
	void *context,
	int flags
	)
{
	GP_WRAP_START;
	{
		return query_tree_mutator(query, mutator, context, flags);
	}
	GP_WRAP_END;
	return NULL;
}

List *
gpdb::PlMutateRangeTable
	(
	List *rtable,
	Node *(*mutator) (),
	void *context,
	int flags
	)
{
	GP_WRAP_START;
	{
		return range_table_mutator(rtable, mutator, context, flags);
	}
	GP_WRAP_END;
	return NIL;
}

bool
gpdb::FRelPartIsRoot
	(
	Oid relid
	)
{
	GP_WRAP_START;
	{
		return PART_STATUS_ROOT == rel_part_status(relid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FRelPartIsInterior
	(
	Oid relid
	)
{
	GP_WRAP_START;
	{
		return PART_STATUS_INTERIOR == rel_part_status(relid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FRelPartIsNone
	(
	Oid relid
	)
{
	GP_WRAP_START;
	{
		return PART_STATUS_NONE == rel_part_status(relid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FHasSubclass
	(
	Oid oidRel
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_inherits */
		return has_subclass(oidRel);
	}
	GP_WRAP_END;
	return false;
}


bool
gpdb::FHasParquetChildren
	(
	Oid oidRel
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_inherits, pg_class */
		return has_parquet_children(oidRel);
	}
	GP_WRAP_END;
	return false;
}

GpPolicy *
gpdb::Pdistrpolicy
	(
	Relation rel
	)
{
    GP_WRAP_START;
    {
        /* catalog tables: pg_class */
    	return relation_policy(rel);
    }
    GP_WRAP_END;
    return NULL;
}

gpos::BOOL
gpdb::FChildPartDistributionMismatch
	(
	Relation rel
	)
{
    GP_WRAP_START;
    {
    	/* catalog tables: pg_class, pg_inherits */
    	return child_distribution_mismatch(rel);
    }
    GP_WRAP_END;
    return false;
}

gpos::BOOL
gpdb::FChildTriggers
	(
	Oid oid,
	int triggerType
	)
{
    GP_WRAP_START;
    {
		/* catalog tables: pg_inherits, pg_trigger */
    	return child_triggers(oid, triggerType);
    }
    GP_WRAP_END;
    return false;
}

bool
gpdb::FRelationExists
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_class */
		return relation_exists(oid);
	}
	GP_WRAP_END;
	return false;
}

List *
gpdb::PlRelationOids(void)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_class */
		return relation_oids();
	}
	GP_WRAP_END;
	return NIL;
}

void
gpdb::EstimateRelationSize
	(
	Relation rel,
	int32 *attr_widths,
	BlockNumber *pages,
	double *tuples
	)
{
	GP_WRAP_START;
	{
		estimate_rel_size(rel, attr_widths, pages, tuples);
		return;
	}
	GP_WRAP_END;
}

void
gpdb::CloseRelation
	(
	Relation rel
	)
{
	GP_WRAP_START;
	{
		RelationClose(rel);
		return;
	}
	GP_WRAP_END;
}

List *
gpdb::PlRelationIndexes
	(
	Relation relation
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: from relcache */
		return RelationGetIndexList(relation);
	}
	GP_WRAP_END;
	return NIL;
}

LogicalIndexes *
gpdb::Plgidx
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rule, pg_index */
		return BuildLogicalIndexInfo(oid);
	}
	GP_WRAP_END;
	return NULL;
}

LogicalIndexInfo *
gpdb::Plgidxinfo
	(
	Oid rootOid, 
	Oid indexOid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_index */
		return logicalIndexInfoForIndexOid(rootOid, indexOid);
	}
	GP_WRAP_END;
	return NULL;
}

void
gpdb::BuildRelationTriggers
	(
	Relation rel
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_trigger */
		RelationBuildTriggers(rel);
		return;
	}
	GP_WRAP_END;
}

Relation
gpdb::RelGetRelation
	(
	Oid relationId
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: relcache */
		return RelationIdGetRelation(relationId);
	}
	GP_WRAP_END;
	return NULL;
}

ExtTableEntry *
gpdb::Pexttable
	(
	Oid relationId
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_exttable */
		return GetExtTableEntry(relationId);
	}
	GP_WRAP_END;
	return NULL;
}

List *
gpdb::PlExternalScanUriList
	(
	ExtTableEntry *ext,
	bool *ismasteronlyp
	)
{
	GP_WRAP_START;
	{
		return create_external_scan_uri_list(ext, ismasteronlyp);
	}
	GP_WRAP_END;
	return NULL;
}

TargetEntry *
gpdb::PteMember
	(
	Node *pnode,
	List *targetlist
	)
{
	GP_WRAP_START;
	{
		return tlist_member(pnode, targetlist);
	}
	GP_WRAP_END;
	return NULL;
}

List *
gpdb::PteMembers
	(
	Node *pnode,
	List *targetlist
	)
{
	GP_WRAP_START;
	{
		return tlist_members(pnode, targetlist);
	}
	GP_WRAP_END;

	return NIL;
}

bool
gpdb::FEqual
	(
	void *p1,
	void *p2
	)
{
	GP_WRAP_START;
	{
		return equal(p1, p2);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FTypeExists
	(
	Oid oid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		return type_exists(oid);
	}
	GP_WRAP_END;
	return false;
}

bool
gpdb::FCompositeType
	(
	Oid typid
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type */
		return type_is_rowtype(typid);
	}
	GP_WRAP_END;
	return false;
}

int
gpdb::IValue
	(
	Node *pnode
	)
{
	GP_WRAP_START;
	{
		return intVal(pnode);
	}
	GP_WRAP_END;
	return 0;
}

Uri *
gpdb::PuriParseExternalTable
	(
	const char *szUri
	)
{
	GP_WRAP_START;
	{
		return ParseExternalTableUri(szUri);
	}
	GP_WRAP_END;
	return NULL;
}

CdbComponentDatabases *
gpdb::PcdbComponentDatabases(void)
{
	GP_WRAP_START;
	{
		/* catalog tables: gp_segment_config */
		return getCdbComponentDatabases();
	}
	GP_WRAP_END;
	return NULL;
}

int
gpdb::IStrCmpIgnoreCase
	(
	const char *sz1,
	const char *sz2
	)
{
	GP_WRAP_START;
	{
		return pg_strcasecmp(sz1, sz2);
	}
	GP_WRAP_END;
	return 0;
}

bool *
gpdb::RgfRandomSegMap
	(
	int total_primaries,
	int total_to_skip
	)
{
	GP_WRAP_START;
	{
		return makeRandomSegMap(total_primaries, total_to_skip);
	}
	GP_WRAP_END;
	return NULL;
}

StringInfo
gpdb::SiMakeStringInfo(void)
{
	GP_WRAP_START;
	{
		return makeStringInfo();
	}
	GP_WRAP_END;
	return NULL;
}

void
gpdb::AppendStringInfo
	(
	StringInfo str,
	const char *str1,
	const char *str2
	)
{
	GP_WRAP_START;
	{
		appendStringInfo(str, "%s%s", str1, str2);
		return;
	}
	GP_WRAP_END;
}

int
gpdb::IFindNodes
	(
	Node *node,
	List *nodeTags
	)
{
	GP_WRAP_START;
	{
		return find_nodes(node, nodeTags);
	}
	GP_WRAP_END;
	return -1;
}

Node *
gpdb::PnodeCoerceToCommonType
	(
	ParseState *pstate,
	Node *pnode,
	Oid oidTargetType,
	const char *context
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_type, pg_cast */
		return coerce_to_common_type
					(
					pstate,
					pnode,
					oidTargetType,
					context
					);
	}
	GP_WRAP_END;
	return NULL;
}

bool
gpdb::FResolvePolymorphicType
	(
	int numargs,
	Oid *argtypes,
	char *argmodes,
	FuncExpr *call_expr
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_proc */
		return resolve_polymorphic_argtypes(numargs, argtypes, argmodes, (Node *)call_expr);
	}
	GP_WRAP_END;
	return false;
}

// hash a const value with GPDB's hash function
int32 
gpdb::ICdbHash
	(
	Const *pconst,
	int iSegments
	)
{
	GP_WRAP_START;
	{
		return cdbhash_const(pconst, iSegments);	
	}
	GP_WRAP_END;
	return 0;
}

// hash a list of const values with GPDB's hash function
int32 
gpdb::ICdbHashList
	(
	List *plConsts,
	int iSegments
	)
{
	GP_WRAP_START;
	{
		return cdbhash_const_list(plConsts, iSegments);	
	}
	GP_WRAP_END;
	return 0;
}

// check permissions on range table
void
gpdb::CheckRTPermissions
	(
	List *plRangeTable
	)
{
	GP_WRAP_START;
	{
		ExecCheckRTPerms(plRangeTable);
		return;
	}
	GP_WRAP_END;
}

// get index op family properties
void
gpdb::IndexOpProperties
	(
	Oid opno,
	Oid opfamily,
	int *strategy,
	Oid *subtype
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_amop */

		// Only the right type is returned to the caller, the left
		// type is simply ignored.
		Oid	lefttype;

		get_op_opfamily_properties(opno, opfamily, strategy, &lefttype, subtype);
		return;
	}
	GP_WRAP_END;
}

// get oids of opfamilies for the index keys
List *
gpdb::PlIndexOpFamilies
	(
	Oid oidIndex
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_index */

		// We return the operator families of the index keys.
		return get_index_opfamilies(oidIndex);
	}
	GP_WRAP_END;
	
	return NIL;
}

// get oids of families this operator belongs to
List *
gpdb::PlScOpOpFamilies
	(
	Oid opno
	)
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_amop */

		// We return the operator families this operator
		// belongs to.
		return get_operator_opfamilies(opno);
	}
	GP_WRAP_END;
	
	return NIL;
}



// Evaluates 'pexpr' and returns the result as an Expr.
// Caller keeps ownership of 'pexpr' and takes ownership of the result
Expr *
gpdb::PexprEvaluate
	(
	Expr *pexpr,
	Oid oidResultType,
	int32 iTypeMod
	)
{
	GP_WRAP_START;
	{
		return evaluate_expr(pexpr, oidResultType, iTypeMod);
	}
	GP_WRAP_END;
	return NULL;
}

// interpret the value of "With oids" option from a list of defelems
bool
gpdb::FInterpretOidsOption
	(
	List *plOptions
	)
{
	GP_WRAP_START;
	{
		return interpretOidsOption(plOptions);
	}
	GP_WRAP_END;
	return false;
}

char *
gpdb::SzDefGetString
	(
	DefElem *pdefelem
	)
{
	GP_WRAP_START;
	{
		return defGetString(pdefelem);
	}
	GP_WRAP_END;
	return NULL;
}

Expr *
gpdb::PexprTransformArrayConstToArrayExpr
	(
	Const *c
	)
{
	GP_WRAP_START;
	{
		return transform_array_Const_to_ArrayExpr(c);
	}
	GP_WRAP_END;
	return NULL;
}

Node *
gpdb::PnodeEvalConstExpressions
	(
	Node *node
	)
{
	GP_WRAP_START;
	{
		return eval_const_expressions(NULL, node);
	}
	GP_WRAP_END;
	return NULL;
}

SelectedParts *
gpdb::SpStaticPartitionSelection
	(
	PartitionSelector *ps
	)
{
	GP_WRAP_START;
	{
		return static_part_selection(ps);
	}
	GP_WRAP_END;
	return NULL;
}

FaultInjectorType_e
gpdb::OptTasksFaultInjector
	(
	FaultInjectorIdentifier_e identifier
	)
{
	// use gpfaultinjector to activate
	// e.g. gpfaultinjector -f opt_task_allocate_string_buffer -y <fault_type> --seg_dbid 1
	// use 'reset' as <fault_type> to clear injected fault
	GP_WRAP_START;
	{
		return FaultInjector_InjectFaultIfSet(identifier, DDLNotSpecified, "", "");
	}
	GP_WRAP_END;
	return FaultInjectorTypeNotSpecified;
}

gpos::ULONG
gpdb::UlLeafPartitions
       (
       Oid oidRelation
       )
{
	GP_WRAP_START;
	{
		/* catalog tables: pg_partition, pg_partition_rules */
		return countLeafPartTables(oidRelation);
	}
	GP_WRAP_END;

	return 0;
}

/*
 * To detect changes to catalog tables that require resetting the Metadata
 * Cache, we use the normal PostgreSQL catalog cache invalidation mechanism.
 * We register a callback to a cache on all the catalog tables that contain
 * information that's contained in the ORCA metadata cache.

 * There is no fine-grained mechanism in the metadata cache for invalidating
 * individual entries ATM, so we just blow the whole cache whenever anything
 * changes. The callback simply increments a counter. Whenever we start
 * planning a query, we check the counter to see if it has changed since the
 * last planned query, and reset the whole cache if it has.
 *
 * To make sure we've covered all catalog tables that contain information
 * that's stored in the metadata cache, there are "catalog tables: xxx"
 * comments in all the calls to backend functions in this file. They indicate
 * which catalog tables each function uses. We conservatively assume that
 * anything fetched via the wrapper functions in this file can end up in the
 * metadata cache and hence need to have an invalidation callback registered.
 */
static bool mdcache_invalidation_counter_registered = false;
static int64 mdcache_invalidation_counter = 0;
static int64 last_mdcache_invalidation_counter = 0;

static void
mdsyscache_invalidation_counter_callback(Datum arg, int cacheid,  ItemPointer tuplePtr)
{
	mdcache_invalidation_counter++;
}

static void
mdrelcache_invalidation_counter_callback(Datum arg, Oid relid)
{
	mdcache_invalidation_counter++;
}

static void
register_mdcache_invalidation_callbacks(void)
{
	/* These are all the catalog tables that we care about. */
	int			metadata_caches[] = {
		AGGFNOID,			/* pg_aggregate */
		AMOPOPID,			/* pg_amop */
		CASTSOURCETARGET,	/* pg_cast */
		CONSTROID,			/* pg_constraint */
		OPEROID,			/* pg_operator */
		OPFAMILYOID,		/* pg_opfamily */
		PARTOID,			/* pg_partition */
		PARTRULEOID,		/* pg_partition_rule */
		STATRELATT,			/* pg_statistics */
		TYPEOID,			/* pg_type */
		PROCOID,			/* pg_proc */

		/*
		 * lookup_type_cache() will also access pg_opclass, via GetDefaultOpClass(),
		 * but there is no syscache for it. Postgres doesn't seem to worry about
		 * invalidating the type cache on updates to pg_opclass, so we don't
		 * worry about that either.
		 */
		/* pg_opclass */

		/*
		 * Information from the following catalogs are included in the
		 * relcache, and any updates will generate relcache invalidation
		 * event. We'll catch the relcache invalidation event and don't need
		 * to register a catcache callback for them.
		 */
		/* pg_class */
		/* pg_index */
		/* pg_trigger */

		/*
		 * pg_exttable is only updated when a new external table is dropped/created,
		 * which will trigger a relcache invalidation event.
		 */
		/* pg_exttable */

		/*
		 * XXX: no syscache on pg_inherits. Is that OK? For any partitioning
		 * changes, I think there will also be updates on pg_partition and/or
		 * pg_partition_rules.
		 */
		/* pg_inherits */

		/*
		 * We assume that gp_segment_config will not change on the fly in a way that
		 * would affect ORCA
		 */
		/* gp_segment_config */
	};
	unsigned int i;

	for (i = 0; i < lengthof(metadata_caches); i++)
	{
		CacheRegisterSyscacheCallback(metadata_caches[i],
									  &mdsyscache_invalidation_counter_callback,
									  (Datum) 0);
	}

	/* also register the relcache callback */
	CacheRegisterRelcacheCallback(&mdrelcache_invalidation_counter_callback,
								  (Datum) 0);
}

// Has there been any catalog changes since last call?
bool
gpdb::FMDCacheNeedsReset
		(
			void
		)
{
	GP_WRAP_START;
	{
		if (!mdcache_invalidation_counter_registered)
		{
			register_mdcache_invalidation_callbacks();
			mdcache_invalidation_counter_registered = true;
		}
		if (last_mdcache_invalidation_counter == mdcache_invalidation_counter)
			return false;
		else
		{
			last_mdcache_invalidation_counter = mdcache_invalidation_counter;
			return true;
		}
	}
	GP_WRAP_END;

	return true;
}

// Functions for ORCA's memory consumption to be tracked by GPDB
void *
gpdb::OptimizerAlloc
		(
			size_t size
		)
{
	GP_WRAP_START;
	{
		return Ext_OptimizerAlloc(size);
	}
	GP_WRAP_END;

	return NULL;
}

void
gpdb::OptimizerFree
		(
			void *ptr
		)
{
	GP_WRAP_START;
	{
		Ext_OptimizerFree(ptr);
	}
	GP_WRAP_END;
}

// EOF
