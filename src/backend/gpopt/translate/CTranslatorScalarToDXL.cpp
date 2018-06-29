//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CTranslatorScalarToDXL.cpp
//
//	@doc:
//		Implementing the methods needed to translate a GPDB Scalar Operation (in a Query / PlStmt object)
//		into a DXL trees
//
//	@test:
//
//---------------------------------------------------------------------------

#include "postgres.h"

#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "utils/datum.h"
#include "utils/date.h"

/*
 * GPDB_91_MERGE_FIXME: This allows us to call numeric_is_nan(). This is probably
 * a violation of some ORCA coding rule, because we don't do this elsewhere...
 */
extern "C" {
#include "utils/numeric.h"
}

#include "gpopt/translate/CTranslatorScalarToDXL.h"
#include "gpopt/translate/CTranslatorQueryToDXL.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CCTEListEntry.h"

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLDatumInt2.h"
#include "naucrates/dxl/operators/CDXLDatumInt4.h"
#include "naucrates/dxl/operators/CDXLDatumInt8.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"

#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CTranslatorScalarToDXL
//
//	@doc:
//		Ctor
//---------------------------------------------------------------------------
CTranslatorScalarToDXL::CTranslatorScalarToDXL
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CIdGenerator *col_id_generator,
	CIdGenerator *cte_id_generator,
	ULONG query_level,
	BOOL is_query_mode,
	HMUlCTEListEntry *cte_entries,
	DXLNodeArray *cte_dxlnode_array
	)
	:
	m_memory_pool(memory_pool),
	m_md_accessor(md_accessor),
	m_col_id_generator(col_id_generator),
	m_cte_id_generator(cte_id_generator),
	m_query_level(query_level),
	m_has_distributed_tables(false),
	m_is_query_mode(is_query_mode),
	m_op_type(EpspotNone),
	m_cte_entries(cte_entries),
	m_cte_producers(cte_dxlnode_array)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::EdxlbooltypeFromGPDBBoolType
//
//	@doc:
//		Return the EdxlBoolExprType for a given GPDB BoolExprType
//---------------------------------------------------------------------------
EdxlBoolExprType
CTranslatorScalarToDXL::EdxlbooltypeFromGPDBBoolType
	(
	BoolExprType boolexprtype
	)
	const
{
	static ULONG mapping[][2] =
		{
		{NOT_EXPR, Edxlnot},
		{AND_EXPR, Edxland},
		{OR_EXPR, Edxlor},
		};

	EdxlBoolExprType type = EdxlBoolExprTypeSentinel;

	const ULONG arity = GPOS_ARRAY_SIZE(mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *elem = mapping[ul];
		if ((ULONG) boolexprtype == elem[0])
		{
			type = (EdxlBoolExprType) elem[1];
			break;
		}
	}

	GPOS_ASSERT(EdxlBoolExprTypeSentinel != type && "Invalid bool expr type");

	return type;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarIdFromVar
//
//	@doc:
//		Create a DXL node for a scalar ident expression from a GPDB Var expression.
//		This function can be used for constructing a scalar ident operator appearing in a
//		base node (e.g. a scan node) or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarIdFromVar
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, Var));
	const Var * var = (Var *) expr;

	if (var->varattno == 0)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Whole-row variable"));
	}

	// column name
	const CWStringBase *str = var_col_id_mapping->GetOptColName(m_query_level, var, m_op_type);

	// column id
	ULONG id;

	if(var->varattno != 0 || EpspotIndexScan == m_op_type || EpspotIndexOnlyScan == m_op_type)
	{
		id = var_col_id_mapping->GetColId(m_query_level, var, m_op_type);
	}
	else
	{
		id = m_col_id_generator->next_id();
	}
	CMDName *mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, str);

	// create a column reference for the given var
	CDXLColRef *dxl_colref = GPOS_NEW(m_memory_pool) CDXLColRef
												(
												m_memory_pool,
												mdname,
												id,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(var->vartype),
												var->vartypmod
												);

	// create the scalar ident operator
	CDXLScalarIdent *scalar_ident = GPOS_NEW(m_memory_pool) CDXLScalarIdent
													(
													m_memory_pool,
													dxl_colref
													);

	// create the DXL node holding the scalar ident operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, scalar_ident);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarOpFromExpr
//
//	@doc:
//		Create a DXL node for a scalar expression from a GPDB expression node.
//		This function can be used for constructing a scalar operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarOpFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping,
	BOOL *has_distributed_tables // output
	)
{
	static const STranslatorElem translators[] =
	{
		{T_Var, &CTranslatorScalarToDXL::CreateScalarIdFromVar},
		{T_OpExpr, &CTranslatorScalarToDXL::CreateScalarOpExprFromExpr},
		{T_ScalarArrayOpExpr, &CTranslatorScalarToDXL::TranslateArrayOpExpr},
		{T_DistinctExpr, &CTranslatorScalarToDXL::CreateScalarDistCmpFromDistExpr},
		{T_Const, &CTranslatorScalarToDXL::CreateScalarConstFromExpr},
		{T_BoolExpr, &CTranslatorScalarToDXL::CreateScalarBoolExprFromExpr},
		{T_BooleanTest, &CTranslatorScalarToDXL::CreateScalarBooleanTestFromExpr},
		{T_CaseExpr, &CTranslatorScalarToDXL::CreateScalarCaseStmtFromExpr},
		{T_CaseTestExpr, &CTranslatorScalarToDXL::CreateScalarCaseTestFromExpr},
		{T_CoalesceExpr, &CTranslatorScalarToDXL::CreateScalarCoalesceFromExpr},
		{T_MinMaxExpr, &CTranslatorScalarToDXL::CreateScalarMinMaxFromExpr},
		{T_FuncExpr, &CTranslatorScalarToDXL::CreateScalarFuncExprFromFuncExpr},
		{T_Aggref, &CTranslatorScalarToDXL::CreateScalarAggrefFromAggref},
		{T_WindowFunc, &CTranslatorScalarToDXL::CreateScalarWindowFunc},
		{T_NullTest, &CTranslatorScalarToDXL::CreateScalarNullTestFromNullTest},
		{T_NullIfExpr, &CTranslatorScalarToDXL::CreateScalarNullIfFromExpr},
		{T_RelabelType, &CTranslatorScalarToDXL::CreateScalarCastFromRelabelType},
		{T_CoerceToDomain, &CTranslatorScalarToDXL::CreateScalarCoerceFromCoerce},
		{T_CoerceViaIO, &CTranslatorScalarToDXL::CreateScalarCoerceFromCoerceViaIO},
		{T_ArrayCoerceExpr, &CTranslatorScalarToDXL::CreateScalarArrayCoerceExprFromExpr},
		{T_SubLink, &CTranslatorScalarToDXL::CreateDXLNodeFromSublink},
		{T_ArrayExpr, &CTranslatorScalarToDXL::TranslateArray},
		{T_ArrayRef, &CTranslatorScalarToDXL::TranslateArrayRef},
	};

	const ULONG num_translators = GPOS_ARRAY_SIZE(translators);
	NodeTag tag = expr->type;

	// if an output variable is provided, we need to reset the member variable
	if (NULL != has_distributed_tables)
	{
		m_has_distributed_tables = false;
	}

	// save old value for distributed tables flag
	BOOL has_distributed_tables_old = m_has_distributed_tables;

	// find translator for the expression type
	ExprToDXLFn func_ptr = NULL;
	for (ULONG ul = 0; ul < num_translators; ul++)
	{
		STranslatorElem elem = translators[ul];
		if (tag == elem.tag)
		{
			func_ptr = elem.func_ptr;
			break;
		}
	}

	if (NULL == func_ptr)
	{
		CHAR *str = (CHAR*) gpdb::NodeToString(const_cast<Expr*>(expr));
		CWStringDynamic *wcstr = CDXLUtils::CreateDynamicStringFromCharArray(m_memory_pool, str);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion, wcstr->GetBuffer());
	}

	CDXLNode *return_node = (this->*func_ptr)(expr, var_col_id_mapping);

	// combine old and current values for distributed tables flag
	m_has_distributed_tables = m_has_distributed_tables || has_distributed_tables_old;

	if (NULL != has_distributed_tables && m_has_distributed_tables)
	{
		*has_distributed_tables = true;
	}

	return return_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarDistCmpFromDistExpr
//
//	@doc:
//		Create a DXL node for a scalar distinct comparison expression from a GPDB
//		DistinctExpr.
//		This function can be used for constructing a scalar comparison operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarDistCmpFromDistExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, DistinctExpr));
	const DistinctExpr *distinct_expr = (DistinctExpr *) expr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::ListLength(distinct_expr->args));

	CDXLNode *left_node = CreateScalarOpFromExpr
							(
							(Expr *) gpdb::ListNth(distinct_expr->args, 0),
							var_col_id_mapping
							);

	CDXLNode *right_node = CreateScalarOpFromExpr
							(
							(Expr *) gpdb::ListNth(distinct_expr->args, 1),
							var_col_id_mapping
							);

	GPOS_ASSERT(NULL != left_node);
	GPOS_ASSERT(NULL != right_node);

	CDXLScalarDistinctComp *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarDistinctComp
														(
														m_memory_pool,
														GPOS_NEW(m_memory_pool) CMDIdGPDB(distinct_expr->opno)
														);

	// create the DXL node holding the scalar distinct comparison operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// add children in the right order
	dxlnode->AddChild(left_node);
	dxlnode->AddChild(right_node);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarCmpFromOpExpr
//
//	@doc:
//		Create a DXL node for a scalar comparison expression from a GPDB OpExpr.
//		This function can be used for constructing a scalar comparison operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCmpFromOpExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, OpExpr));
	const OpExpr *op_expr = (OpExpr *) expr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::ListLength(op_expr->args));

	Expr *left_expr = (Expr *) gpdb::ListNth(op_expr->args, 0);
	Expr *right_expr = (Expr *) gpdb::ListNth(op_expr->args, 1);

	CDXLNode *left_node = CreateScalarOpFromExpr(left_expr, var_col_id_mapping);
	CDXLNode *right_node = CreateScalarOpFromExpr(right_expr, var_col_id_mapping);

	GPOS_ASSERT(NULL != left_node);
	GPOS_ASSERT(NULL != right_node);

	CMDIdGPDB *mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(op_expr->opno);

	// get operator name
	const CWStringConst *str = GetDXLArrayCmpType(mdid);

	CDXLScalarComp *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarComp(m_memory_pool, mdid, GPOS_NEW(m_memory_pool) CWStringConst(str->GetBuffer()));

	// create the DXL node holding the scalar comparison operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// add children in the right order
	dxlnode->AddChild(left_node);
	dxlnode->AddChild(right_node);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarOpExprFromExpr
//
//	@doc:
//		Create a DXL node for a scalar opexpr from a GPDB OpExpr.
//		This function can be used for constructing a scalar opexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarOpExprFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, OpExpr));

	const OpExpr *op_expr = (OpExpr *) expr;

	// check if this is a scalar comparison
	CMDIdGPDB *return_type_mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(((OpExpr *) expr)->opresulttype);
	const IMDType *md_type= m_md_accessor->Pmdtype(return_type_mdid);

	const ULONG num_args = gpdb::ListLength(op_expr->args);

	if (IMDType::EtiBool ==  md_type->GetDatumType() && 2 == num_args)
	{
		return_type_mdid->Release();
		return CreateScalarCmpFromOpExpr(expr, var_col_id_mapping);
	}

	// get operator name and id
	IMDId *mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(op_expr->opno);
	const CWStringConst *str = GetDXLArrayCmpType(mdid);

	CDXLScalarOpExpr *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarOpExpr(m_memory_pool, mdid, return_type_mdid, GPOS_NEW(m_memory_pool) CWStringConst(str->GetBuffer()));

	// create the DXL node holding the scalar opexpr
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// process arguments
	TranslateScalarChildren(dxlnode, op_expr->args, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarNullIfFromExpr
//
//	@doc:
//		Create a DXL node for a scalar nullif from a GPDB Expr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarNullIfFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, NullIfExpr));
	const NullIfExpr *null_if_expr = (NullIfExpr *) expr;

	GPOS_ASSERT(2 == gpdb::ListLength(null_if_expr->args));

	CDXLScalarNullIf *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarNullIf(m_memory_pool, GPOS_NEW(m_memory_pool) CMDIdGPDB(null_if_expr->opno), GPOS_NEW(m_memory_pool) CMDIdGPDB(null_if_expr->opresulttype));

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// process arguments
	TranslateScalarChildren(dxlnode, null_if_expr->args, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::TranslateArrayOpExpr
//
//	@doc:
//		Create a DXL node for a scalar array expression from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::TranslateArrayOpExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	return CreateScalarArrayCompFromExpr(expr, var_col_id_mapping);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarArrayCompFromExpr
//
//	@doc:
//		Create a DXL node for a scalar array comparison from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarArrayCompFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, ScalarArrayOpExpr));
	const ScalarArrayOpExpr *scalar_array_op_expr = (ScalarArrayOpExpr *) expr;

	// process arguments of op expr
	GPOS_ASSERT(2 == gpdb::ListLength(scalar_array_op_expr->args));

	Expr *left_expr = (Expr*) gpdb::ListNth(scalar_array_op_expr->args, 0);
	CDXLNode *left_node = CreateScalarOpFromExpr(left_expr, var_col_id_mapping);

	Expr *right_expr = (Expr*) gpdb::ListNth(scalar_array_op_expr->args, 1);

	// If the argument array is an array Const, try to transform it to an
	// ArrayExpr, to allow ORCA to optimize it better. (ORCA knows how to
	// extract elements of an ArrayExpr, but doesn't currently know how
	// to do it from an array-typed Const.)
	if (IsA(right_expr, Const))
		right_expr = gpdb::TransformArrayConstToArrayExpr((Const *) right_expr);

	CDXLNode *right_node = CreateScalarOpFromExpr(right_expr, var_col_id_mapping);

	GPOS_ASSERT(NULL != left_node);
	GPOS_ASSERT(NULL != right_node);

	// get operator name
	CMDIdGPDB *mdid_op = GPOS_NEW(m_memory_pool) CMDIdGPDB(scalar_array_op_expr->opno);
	const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(mdid_op);
	mdid_op->Release();

	const CWStringConst *op_name = md_scalar_op->Mdname().GetMDName();
	GPOS_ASSERT(NULL != op_name);

	EdxlArrayCompType type = Edxlarraycomptypeany;

	if(!scalar_array_op_expr->useOr)
	{
		type = Edxlarraycomptypeall;
	}

	CDXLScalarArrayComp *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarArrayComp(m_memory_pool, GPOS_NEW(m_memory_pool) CMDIdGPDB(scalar_array_op_expr->opno), GPOS_NEW(m_memory_pool) CWStringConst(op_name->GetBuffer()), type);

	// create the DXL node holding the scalar opexpr
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// add children in the right order
	dxlnode->AddChild(left_node);
	dxlnode->AddChild(right_node);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarConstFromExpr
//
//	@doc:
//		Create a DXL node for a scalar const value from a GPDB Const
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarConstFromExpr
	(
	const Expr *expr,
	const CMappingVarColId * // var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, Const));
	const Const *constant = (Const *) expr;

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarConstValue
												(
												m_memory_pool,
												GetDatumVal(constant)
												)
									);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDatumVal
//
//	@doc:
//		Create a DXL datum from a GPDB Const
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::GetDatumVal
	(
	IMemoryPool *memory_pool,
	CMDAccessor *mda,
	const Const *constant
	)
{
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(constant->consttype);
	const IMDType *md_type= mda->Pmdtype(mdid);
	mdid->Release();

 	// translate gpdb datum into a DXL datum
	CDXLDatum *datum_dxl = CTranslatorScalarToDXL::GetDatumVal(memory_pool, md_type, constant->consttypmod, constant->constisnull,
															 constant->constlen,
															 constant->constvalue);

	return datum_dxl;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDatumVal
//
//	@doc:
//		Create a DXL datum from a GPDB Const
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::GetDatumVal
	(
	const Const *constant
	)
	const
{
	return GetDatumVal(m_memory_pool, m_md_accessor, constant);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarBoolExprFromExpr
//
//	@doc:
//		Create a DXL node for a scalar boolean expression from a GPDB OpExpr.
//		This function can be used for constructing a scalar boolexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarBoolExprFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, BoolExpr));
	const BoolExpr *bool_expr = (BoolExpr *) expr;
	GPOS_ASSERT(0 < gpdb::ListLength(bool_expr->args));

	EdxlBoolExprType type = EdxlbooltypeFromGPDBBoolType(bool_expr->boolop);
	GPOS_ASSERT(EdxlBoolExprTypeSentinel != type);

	// create the DXL node holding the scalar boolean operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarBoolExpr(m_memory_pool, type));

	ULONG count = gpdb::ListLength(bool_expr->args);

	if ((NOT_EXPR != bool_expr->boolop) && (2 > count))
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiPlStmt2DXLConversion,
			GPOS_WSZ_LIT("Boolean Expression (OR / AND): Incorrect Number of Children ")
			);
	}
	else if ((NOT_EXPR == bool_expr->boolop) && (1 != count))
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiPlStmt2DXLConversion,
			GPOS_WSZ_LIT("Boolean Expression (NOT): Incorrect Number of Children ")
			);
	}

	TranslateScalarChildren(dxlnode, bool_expr->args, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarBooleanTestFromExpr
//
//	@doc:
//		Create a DXL node for a scalar boolean test from a GPDB OpExpr.
//		This function can be used for constructing a scalar boolexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarBooleanTestFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, BooleanTest));

	const BooleanTest *boolean_test = (BooleanTest *) expr;

	GPOS_ASSERT(NULL != boolean_test->arg);

	static ULONG mapping[][2] =
		{
		{IS_TRUE, EdxlbooleantestIsTrue},
		{IS_NOT_TRUE, EdxlbooleantestIsNotTrue},
		{IS_FALSE, EdxlbooleantestIsFalse},
		{IS_NOT_FALSE, EdxlbooleantestIsNotFalse},
		{IS_UNKNOWN, EdxlbooleantestIsUnknown},
		{IS_NOT_UNKNOWN, EdxlbooleantestIsNotUnknown},
		};

	EdxlBooleanTestType type = EdxlbooleantestSentinel;
	const ULONG arity = GPOS_ARRAY_SIZE(mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *elem = mapping[ul];
		if ((ULONG) boolean_test->booltesttype == elem[0])
		{
			type = (EdxlBooleanTestType) elem[1];
			break;
		}
	}
	GPOS_ASSERT(EdxlbooleantestSentinel != type && "Invalid boolean test type");

	// create the DXL node holding the scalar boolean test operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarBooleanTest(m_memory_pool,type)
									);

	CDXLNode *dxlnode_arg = CreateScalarOpFromExpr(boolean_test->arg, var_col_id_mapping);
	GPOS_ASSERT(NULL != dxlnode_arg);

	dxlnode->AddChild(dxlnode_arg);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarNullTestFromNullTest
//
//	@doc:
//		Create a DXL node for a scalar nulltest expression from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarNullTestFromNullTest
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, NullTest));
	const NullTest *null_test = (NullTest *) expr;

	GPOS_ASSERT(NULL != null_test->arg);
	CDXLNode *child_node = CreateScalarOpFromExpr(null_test->arg, var_col_id_mapping);

	GPOS_ASSERT(NULL != child_node);
	GPOS_ASSERT(IS_NULL == null_test->nulltesttype || IS_NOT_NULL == null_test->nulltesttype);

	BOOL is_null = false;
	if (IS_NULL == null_test->nulltesttype)
	{
		is_null = true;
	}

	// create the DXL node holding the scalar NullTest operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarNullTest(m_memory_pool, is_null));
	dxlnode->AddChild(child_node);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarCoalesceFromExpr
//
//	@doc:
//		Create a DXL node for a coalesce function from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCoalesceFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, CoalesceExpr));

	CoalesceExpr *coalesce_expr = (CoalesceExpr *) expr;
	GPOS_ASSERT(NULL != coalesce_expr->args);

	CDXLScalarCoalesce *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarCoalesce
											(
											m_memory_pool,
											GPOS_NEW(m_memory_pool) CMDIdGPDB(coalesce_expr->coalescetype)
											);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	TranslateScalarChildren(dxlnode, coalesce_expr->args, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarMinMaxFromExpr
//
//	@doc:
//		Create a DXL node for a min/max operator from a GPDB OpExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarMinMaxFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, MinMaxExpr));

	MinMaxExpr *min_max_expr = (MinMaxExpr *) expr;
	GPOS_ASSERT(NULL != min_max_expr->args);

	CDXLScalarMinMax::EdxlMinMaxType min_max_type = CDXLScalarMinMax::EmmtSentinel;
	if (IS_GREATEST == min_max_expr->op)
	{
		min_max_type = CDXLScalarMinMax::EmmtMax;
	}
	else
	{
		GPOS_ASSERT(IS_LEAST == min_max_expr->op);
		min_max_type = CDXLScalarMinMax::EmmtMin;
	}

	CDXLScalarMinMax *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarMinMax
											(
											m_memory_pool,
											GPOS_NEW(m_memory_pool) CMDIdGPDB(min_max_expr->minmaxtype),
											min_max_type
											);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	TranslateScalarChildren(dxlnode, min_max_expr->args, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::TranslateScalarChildren
//
//	@doc:
//		Translate list elements and add them as children of the DXL node
//---------------------------------------------------------------------------
void
CTranslatorScalarToDXL::TranslateScalarChildren
	(
	CDXLNode *dxlnode,
	List *list,
	const CMappingVarColId* var_col_id_mapping,
	BOOL *has_distributed_tables // output
	)
{
	ListCell *lc = NULL;
	ForEach (lc, list)
	{
		Expr *child_expr = (Expr *) lfirst(lc);
		CDXLNode *child_node = CreateScalarOpFromExpr(child_expr, var_col_id_mapping, has_distributed_tables);
		GPOS_ASSERT(NULL != child_node);
		dxlnode->AddChild(child_node);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarCaseStmtFromExpr
//
//	@doc:
//		Create a DXL node for a case statement from a GPDB OpExpr.
//		This function can be used for constructing a scalar opexpr operator appearing in a
//		base node (e.g. a scan node), or in an intermediate plan nodes.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCaseStmtFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, CaseExpr));

	const CaseExpr *case_expr = (CaseExpr *) expr;

	if (NULL == case_expr->args)
	{
			GPOS_RAISE
				(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Do not support SIMPLE CASE STATEMENT")
				);
			return NULL;
	}

	if (NULL == case_expr->arg)
	{
		return CreateScalarIfStmtFromCaseExpr(case_expr, var_col_id_mapping);
	}

	return CreateScalarSwitchFromCaseExpr(case_expr, var_col_id_mapping);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarSwitchFromCaseExpr
//
//	@doc:
//		Create a DXL Switch node from a GPDB CaseExpr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarSwitchFromCaseExpr
	(
	const CaseExpr *case_expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT (NULL != case_expr->arg);

	CDXLScalarSwitch *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarSwitch
												(
												m_memory_pool,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(case_expr->casetype)
												);
	CDXLNode *switch_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// translate the switch expression
	CDXLNode *dxlnode_arg = CreateScalarOpFromExpr(case_expr->arg, var_col_id_mapping);
	switch_node->AddChild(dxlnode_arg);

	// translate the cases
	ListCell *lc = NULL;
	ForEach (lc, case_expr->args)
	{
		CaseWhen *expr = (CaseWhen *) lfirst(lc);

		CDXLScalarSwitchCase *swtich_case = GPOS_NEW(m_memory_pool) CDXLScalarSwitchCase(m_memory_pool);
		CDXLNode *switch_case_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, swtich_case);

		CDXLNode *cmp_expr_node = CreateScalarOpFromExpr(expr->expr, var_col_id_mapping);
		GPOS_ASSERT(NULL != cmp_expr_node);

		CDXLNode *result_node = CreateScalarOpFromExpr(expr->result, var_col_id_mapping);
		GPOS_ASSERT(NULL != result_node);

		switch_case_node->AddChild(cmp_expr_node);
		switch_case_node->AddChild(result_node);

		// add current case to switch node
		switch_node->AddChild(switch_case_node);
	}

	// translate the "else" clause
	if (NULL != case_expr->defresult)
	{
		CDXLNode *default_result_node = CreateScalarOpFromExpr(case_expr->defresult, var_col_id_mapping);
		GPOS_ASSERT(NULL != default_result_node);

		switch_node->AddChild(default_result_node);

	}

	return switch_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarCaseTestFromExpr
//
//	@doc:
//		Create a DXL node for a case test from a GPDB Expr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCaseTestFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* //var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, CaseTestExpr));
	const CaseTestExpr *case_test_expr = (CaseTestExpr *) expr;
	CDXLScalarCaseTest *dxlop = GPOS_NEW(m_memory_pool) CDXLScalarCaseTest
												(
												m_memory_pool,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(case_test_expr->typeId)
												);

	return GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarIfStmtFromCaseExpr
//
//	@doc:
//		Create a DXL If node from a GPDB CaseExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarIfStmtFromCaseExpr
	(
	const CaseExpr *case_expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT (NULL == case_expr->arg);
	const ULONG when_clause_count = gpdb::ListLength(case_expr->args);

	CDXLNode *root_if_tree_node = NULL;
	CDXLNode *cur_node = NULL;

	for (ULONG ul = 0; ul < when_clause_count; ul++)
	{
		CDXLScalarIfStmt *if_stmt_new_dxl = GPOS_NEW(m_memory_pool) CDXLScalarIfStmt
															(
															m_memory_pool,
															GPOS_NEW(m_memory_pool) CMDIdGPDB(case_expr->casetype)
															);

		CDXLNode *if_stmt_new_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, if_stmt_new_dxl);

		CaseWhen *expr = (CaseWhen *) gpdb::ListNth(case_expr->args, ul);
		GPOS_ASSERT(IsA(expr, CaseWhen));

		CDXLNode *cond_node = CreateScalarOpFromExpr(expr->expr, var_col_id_mapping);
		CDXLNode *result_node = CreateScalarOpFromExpr(expr->result, var_col_id_mapping);

		GPOS_ASSERT(NULL != cond_node);
		GPOS_ASSERT(NULL != result_node);

		if_stmt_new_node->AddChild(cond_node);
		if_stmt_new_node->AddChild(result_node);

		if(NULL == root_if_tree_node)
		{
			root_if_tree_node = if_stmt_new_node;
		}
		else
		{
			cur_node->AddChild(if_stmt_new_node);
		}
		cur_node = if_stmt_new_node;
	}

	if (NULL != case_expr->defresult)
	{
		CDXLNode *default_result_node = CreateScalarOpFromExpr(case_expr->defresult, var_col_id_mapping);
		GPOS_ASSERT(NULL != default_result_node);
		cur_node->AddChild(default_result_node);
	}

	return root_if_tree_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarCastFromRelabelType
//
//	@doc:
//		Create a DXL node for a scalar RelabelType expression from a GPDB RelabelType
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCastFromRelabelType
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, RelabelType));

	const RelabelType *relabel_type = (RelabelType *) expr;

	GPOS_ASSERT(NULL != relabel_type->arg);

	CDXLNode *child_node = CreateScalarOpFromExpr(relabel_type->arg, var_col_id_mapping);

	GPOS_ASSERT(NULL != child_node);

	// create the DXL node holding the scalar boolean operator
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarCast
												(
												m_memory_pool,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(relabel_type->resulttype),
												GPOS_NEW(m_memory_pool) CMDIdGPDB(0) // casting function oid
												)
									);
	dxlnode->AddChild(child_node);

	return dxlnode;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorScalarToDXL::CreateScalarCoerceFromCoerce
//
//      @doc:
//              Create a DXL node for a scalar coerce expression from a
//             GPDB coerce expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCoerceFromCoerce
        (
        const Expr *expr,
        const CMappingVarColId* var_col_id_mapping
        )
{
        GPOS_ASSERT(IsA(expr, CoerceToDomain));

        const CoerceToDomain *coerce = (CoerceToDomain *) expr;

        GPOS_ASSERT(NULL != coerce->arg);

        CDXLNode *child_node = CreateScalarOpFromExpr(coerce->arg, var_col_id_mapping);

        GPOS_ASSERT(NULL != child_node);

        // create the DXL node holding the scalar boolean operator
        CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
                                                                        (
                                                                        m_memory_pool,
                                                                        GPOS_NEW(m_memory_pool) CDXLScalarCoerceToDomain
                                                                                                (
                                                                                                m_memory_pool,
                                                                                                GPOS_NEW(m_memory_pool) CMDIdGPDB(coerce->resulttype),
                                                                                               coerce->resulttypmod,
                                                                                               (EdxlCoercionForm) coerce->coercionformat,
                                                                                               coerce->location
                                                                                                )
                                                                        );
        dxlnode->AddChild(child_node);

        return dxlnode;
}

//---------------------------------------------------------------------------
//      @function:
//              CTranslatorScalarToDXL::CreateScalarCoerceFromCoerceViaIO
//
//      @doc:
//              Create a DXL node for a scalar coerce expression from a
//             GPDB coerce expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCoerceFromCoerceViaIO
        (
        const Expr *expr,
        const CMappingVarColId* var_col_id_mapping
        )
{
        GPOS_ASSERT(IsA(expr, CoerceViaIO));

        const CoerceViaIO *coerce = (CoerceViaIO *) expr;

        GPOS_ASSERT(NULL != coerce->arg);

        CDXLNode *child_node = CreateScalarOpFromExpr(coerce->arg, var_col_id_mapping);

        GPOS_ASSERT(NULL != child_node);

        // create the DXL node holding the scalar boolean operator
        CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
                                                                        (
                                                                        m_memory_pool,
                                                                        GPOS_NEW(m_memory_pool) CDXLScalarCoerceViaIO
                                                                                                (
                                                                                                m_memory_pool,
                                                                                                GPOS_NEW(m_memory_pool) CMDIdGPDB(coerce->resulttype),
                                                                                               -1,
                                                                                               (EdxlCoercionForm) coerce->coerceformat,
                                                                                               coerce->location
                                                                                                )
                                                                        );
        dxlnode->AddChild(child_node);

        return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarArrayCoerceExprFromExpr
//	@doc:
//		Create a DXL node for a scalar array coerce expression from a
// 		GPDB Array Coerce expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarArrayCoerceExprFromExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, ArrayCoerceExpr));
	const ArrayCoerceExpr *array_coerce_expr = (ArrayCoerceExpr *) expr;
	
	GPOS_ASSERT(NULL != array_coerce_expr->arg);
	
	CDXLNode *child_node = CreateScalarOpFromExpr(array_coerce_expr->arg, var_col_id_mapping);
	
	GPOS_ASSERT(NULL != child_node);
	
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
					(
					m_memory_pool,
					GPOS_NEW(m_memory_pool) CDXLScalarArrayCoerceExpr
							(
							m_memory_pool,
							GPOS_NEW(m_memory_pool) CMDIdGPDB(array_coerce_expr->elemfuncid),
							GPOS_NEW(m_memory_pool) CMDIdGPDB(array_coerce_expr->resulttype),
							array_coerce_expr->resulttypmod,
							array_coerce_expr->isExplicit,
							(EdxlCoercionForm) array_coerce_expr->coerceformat,
							array_coerce_expr->location
							)
					);
	
        dxlnode->AddChild(child_node);

        return dxlnode;
}




//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarFuncExprFromFuncExpr
//
//	@doc:
//		Create a DXL node for a scalar funcexpr from a GPDB FuncExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarFuncExprFromFuncExpr
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, FuncExpr));
	const FuncExpr *func_expr = (FuncExpr *) expr;
	int32 type_modifier = gpdb::ExprTypeMod((Node *) expr);

	CMDIdGPDB *mdid_func = GPOS_NEW(m_memory_pool) CMDIdGPDB(func_expr->funcid);

	// create the DXL node holding the scalar funcexpr
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode
									(
									m_memory_pool,
									GPOS_NEW(m_memory_pool) CDXLScalarFuncExpr
												(
												m_memory_pool,
												mdid_func,
												GPOS_NEW(m_memory_pool) CMDIdGPDB(func_expr->funcresulttype),
												type_modifier,
												func_expr->funcretset
												)
									);

	const IMDFunction *md_func = m_md_accessor->Pmdfunc(mdid_func);
	if (IMDFunction::EfsVolatile == md_func->GetFuncStability())
	{
		ListCell *lc = NULL;
		ForEach (lc, func_expr->args)
		{
			Node *arg_node = (Node *) lfirst(lc);
			if (CTranslatorUtils::HasSubquery(arg_node))
			{
				GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature,
						GPOS_WSZ_LIT("Volatile functions with subqueries in arguments"));
			}
		}
	}

	TranslateScalarChildren(dxlnode, func_expr->args, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarAggrefFromAggref
//
//	@doc:
//		Create a DXL node for a scalar aggref from a GPDB FuncExpr
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarAggrefFromAggref
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, Aggref));
	const Aggref *aggref = (Aggref *) expr;
	BOOL is_distinct = false;

	static ULONG mapping[][2] =
		{
		{AGGSTAGE_NORMAL, EdxlaggstageNormal},
		{AGGSTAGE_PARTIAL, EdxlaggstagePartial},
		{AGGSTAGE_INTERMEDIATE, EdxlaggstageIntermediate},
		{AGGSTAGE_FINAL, EdxlaggstageFinal},
		};

	if (aggref->aggorder != NIL)
	{
		GPOS_RAISE
		(
				gpdxl::ExmaDXL,
				gpdxl::ExmiPlStmt2DXLConversion,
				GPOS_WSZ_LIT("Ordered aggregates")
		);
	}

	if (aggref->aggdistinct)
	{
		is_distinct = true;
	}

	EdxlAggrefStage agg_stage = EdxlaggstageSentinel;
	const ULONG arity = GPOS_ARRAY_SIZE(mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *elem = mapping[ul];
		if ((ULONG) aggref->aggstage == elem[0])
		{
			agg_stage = (EdxlAggrefStage) elem[1];
			break;
		}
	}
	GPOS_ASSERT(EdxlaggstageSentinel != agg_stage && "Invalid agg stage");

	CMDIdGPDB *agg_mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(aggref->aggfnoid);
	const IMDAggregate *md_agg = m_md_accessor->Pmdagg(agg_mdid);

	GPOS_ASSERT(!md_agg->IsOrdered());

	if (0 != aggref->agglevelsup)
	{
		// TODO: Feb 05 2015, remove temporary fix to avoid erroring out during execution
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLError, GPOS_WSZ_LIT("Aggregate functions with outer references"));
	}

	// ORCA doesn't support the FILTER clause yet.
	if (aggref->aggfilter)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Aggregate functions with FILTER"));
	}

	IMDId *mdid_return_type = CScalarAggFunc::PmdidLookupReturnType(agg_mdid, (EdxlaggstageNormal == agg_stage), m_md_accessor);
	IMDId *resolved_ret_type = NULL;
	if (m_md_accessor->Pmdtype(mdid_return_type)->IsAmbiguous())
	{
		// if return type given by MD cache is ambiguous, use type provided by aggref node
		resolved_ret_type = GPOS_NEW(m_memory_pool) CMDIdGPDB(aggref->aggtype);
	}

	CDXLScalarAggref *aggref_scalar = GPOS_NEW(m_memory_pool) CDXLScalarAggref(m_memory_pool, agg_mdid, resolved_ret_type, is_distinct, agg_stage);

	// create the DXL node holding the scalar aggref
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, aggref_scalar);

	// translate args
	ListCell *lc;
	ForEach (lc, aggref->args)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		CDXLNode *child_node = CreateScalarOpFromExpr(tle->expr, var_col_id_mapping, NULL);
		GPOS_ASSERT(NULL != child_node);
		dxlnode->AddChild(child_node);
	}

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetWindowFrame
//
//	@doc:
//		Create a DXL window frame from a GPDB WindowFrame
//---------------------------------------------------------------------------
CDXLWindowFrame *
CTranslatorScalarToDXL::GetWindowFrame
	(
	int frame_options,
	const Node *start_offset,
	const Node *end_offset,
	const CMappingVarColId* var_col_id_mapping,
	CDXLNode *new_scalar_proj_list,
	BOOL *has_distributed_tables // output
	)
{
	EdxlFrameSpec frame_spec;

	if ((frame_options & FRAMEOPTION_ROWS) != 0)
		frame_spec = EdxlfsRow;
	else
		frame_spec = EdxlfsRange;

	EdxlFrameBoundary leading_boundary;
	if ((frame_options & FRAMEOPTION_END_UNBOUNDED_PRECEDING) != 0)
		leading_boundary = EdxlfbUnboundedPreceding;
	else if ((frame_options & FRAMEOPTION_END_VALUE_PRECEDING) != 0)
		leading_boundary = EdxlfbBoundedPreceding;
	else if ((frame_options & FRAMEOPTION_END_CURRENT_ROW) != 0)
		leading_boundary = EdxlfbCurrentRow;
	else if ((frame_options & FRAMEOPTION_END_VALUE_FOLLOWING) != 0)
		leading_boundary = EdxlfbBoundedFollowing;
	else if ((frame_options & FRAMEOPTION_END_UNBOUNDED_FOLLOWING) != 0)
		leading_boundary = EdxlfbUnboundedFollowing;
	else
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion,
			   GPOS_WSZ_LIT("Unrecognized window frame option"));

	EdxlFrameBoundary trailing_boundary;
	if ((frame_options & FRAMEOPTION_START_UNBOUNDED_PRECEDING) != 0)
		trailing_boundary = EdxlfbUnboundedPreceding;
	else if ((frame_options & FRAMEOPTION_START_VALUE_PRECEDING) != 0)
		trailing_boundary = EdxlfbBoundedPreceding;
	else if ((frame_options & FRAMEOPTION_START_CURRENT_ROW) != 0)
		trailing_boundary = EdxlfbCurrentRow;
	else if ((frame_options & FRAMEOPTION_START_VALUE_FOLLOWING) != 0)
		trailing_boundary = EdxlfbBoundedFollowing;
	else if ((frame_options & FRAMEOPTION_START_UNBOUNDED_FOLLOWING) != 0)
		trailing_boundary = EdxlfbUnboundedFollowing;
	else
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion,
			   GPOS_WSZ_LIT("Unrecognized window frame option"));

	// We don't support non-default EXCLUDE [CURRENT ROW | GROUP | TIES |
	// NO OTHERS] options.
	EdxlFrameExclusionStrategy strategy = EdxlfesNulls;

	CDXLNode *lead_edge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, true /* fLeading */, leading_boundary));
	CDXLNode *trail_edge = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarWindowFrameEdge(m_memory_pool, false /* fLeading */, trailing_boundary));

	// translate the lead and trail value
	if (NULL != end_offset)
	{
		lead_edge->AddChild(CreateDXLNodeFromWindowFrameEdgeVal(end_offset, var_col_id_mapping, new_scalar_proj_list, has_distributed_tables));
	}

	if (NULL != start_offset)
	{
		trail_edge->AddChild(CreateDXLNodeFromWindowFrameEdgeVal(start_offset, var_col_id_mapping, new_scalar_proj_list, has_distributed_tables));
	}

	CDXLWindowFrame *window_frame_dxl = GPOS_NEW(m_memory_pool) CDXLWindowFrame(m_memory_pool, frame_spec, strategy, lead_edge, trail_edge);

	return window_frame_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateDXLNodeFromWindowFrameEdgeVal
//
//	@doc:
//		Translate the window frame edge, if the column used in the edge is a
// 		computed column then add it to the project list
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateDXLNodeFromWindowFrameEdgeVal
	(
	const Node *node,
	const CMappingVarColId* var_col_id_mapping,
	CDXLNode *new_scalar_proj_list,
	BOOL *has_distributed_tables
	)
{
	CDXLNode *val_node = CreateScalarOpFromExpr((Expr *) node, var_col_id_mapping, has_distributed_tables);

	if (m_is_query_mode && !IsA(node, Var) && !IsA(node, Const))
	{
		GPOS_ASSERT(NULL != new_scalar_proj_list);
		CWStringConst unnamed_col(GPOS_WSZ_LIT("?column?"));
		CMDName *alias_mdname = GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &unnamed_col);
		ULONG project_element_id = m_col_id_generator->next_id();

		// construct a projection element
		CDXLNode *project_element_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarProjElem(m_memory_pool, project_element_id, alias_mdname));
		project_element_node->AddChild(val_node);

		// add it to the computed columns project list
		new_scalar_proj_list->AddChild(project_element_node);

		// construct a new scalar ident
		CDXLScalarIdent *scalar_ident = GPOS_NEW(m_memory_pool) CDXLScalarIdent
													(
													m_memory_pool,
													GPOS_NEW(m_memory_pool) CDXLColRef
																(
																m_memory_pool,
																GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, &unnamed_col),
																project_element_id,
																GPOS_NEW(m_memory_pool) CMDIdGPDB(gpdb::ExprType(const_cast<Node*>(node))),
																gpdb::ExprTypeMod(const_cast<Node*>(node))
																)
													);

		val_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, scalar_ident);
	}

	return val_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarWindowFunc
//
//	@doc:
//		Create a DXL node for a scalar window ref from a GPDB WindowFunc
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarWindowFunc
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, WindowFunc));

	const WindowFunc *window_func = (WindowFunc *) expr;

	static ULONG mapping[][2] =
		{
		{WINSTAGE_IMMEDIATE, EdxlwinstageImmediate},
		{WINSTAGE_PRELIMINARY, EdxlwinstagePreliminary},
		{WINSTAGE_ROWKEY, EdxlwinstageRowKey},
		};

	// ORCA doesn't support the FILTER clause yet.
	if (window_func->aggfilter)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Aggregate functions with FILTER"));
	}

	const ULONG arity = GPOS_ARRAY_SIZE(mapping);
	EdxlWinStage dxl_win_stage = EdxlwinstageSentinel;

	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *elem = mapping[ul];
		if ((ULONG) window_func->winstage == elem[0])
		{
			dxl_win_stage = (EdxlWinStage) elem[1];
			break;
		}
	}

	ULONG win_spec_pos = (ULONG) 0;
	if (m_is_query_mode)
	{
		win_spec_pos = (ULONG) window_func->winref - 1;
	}

	GPOS_ASSERT(EdxlwinstageSentinel != dxl_win_stage && "Invalid window stage");

	/*
	 * ORCA's ScalarWindowRef object doesn't have fields for the 'winstar'
	 * and 'winagg', so we lose that information in the translation.
	 * Fortunately, the executor currently doesn't need those fields to
	 * be set correctly.
	 */
	CDXLScalarWindowRef *pdxlopWinref = GPOS_NEW(m_memory_pool) CDXLScalarWindowRef
													(
													m_memory_pool,
													GPOS_NEW(m_memory_pool) CMDIdGPDB(window_func->winfnoid),
													GPOS_NEW(m_memory_pool) CMDIdGPDB(window_func->wintype),
													window_func->windistinct,
													window_func->winstar,
													window_func->winagg,
													dxl_win_stage,
													win_spec_pos
													);

	// create the DXL node holding the scalar aggref
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopWinref);

	TranslateScalarChildren(dxlnode, window_func->args, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarCondFromQual
//
//	@doc:
//		Create a DXL scalar boolean operator node from a GPDB qual list.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarCondFromQual
	(
	List *quals,
	const CMappingVarColId* var_col_id_mapping,
	BOOL *has_distributed_tables
	)
{
	if (NULL == quals || 0 == gpdb::ListLength(quals))
	{
		return NULL;
	}

	if (1 == gpdb::ListLength(quals))
	{
		Expr *expr = (Expr *) gpdb::ListNth(quals, 0);
		return CreateScalarOpFromExpr(expr, var_col_id_mapping, has_distributed_tables);
	}
	else
	{
		// GPDB assumes that if there are a list of qual conditions then it is an implicit AND operation
		// Here we build the left deep AND tree
		CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarBoolExpr(m_memory_pool, Edxland));

		TranslateScalarChildren(dxlnode, quals, var_col_id_mapping, has_distributed_tables);

		return dxlnode;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateFilterFromQual
//
//	@doc:
//		Create a DXL scalar filter node from a GPDB qual list.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateFilterFromQual
	(
	List *quals,
	const CMappingVarColId* var_col_id_mapping,
	Edxlopid filter_type,
	BOOL *has_distributed_tables // output
	)
{
	CDXLScalarFilter *dxlop = NULL;

	switch (filter_type)
	{
		case EdxlopScalarFilter:
			dxlop = GPOS_NEW(m_memory_pool) CDXLScalarFilter(m_memory_pool);
			break;
		case EdxlopScalarJoinFilter:
			dxlop = GPOS_NEW(m_memory_pool) CDXLScalarJoinFilter(m_memory_pool);
			break;
		case EdxlopScalarOneTimeFilter:
			dxlop = GPOS_NEW(m_memory_pool) CDXLScalarOneTimeFilter(m_memory_pool);
			break;
		default:
			GPOS_ASSERT(!"Unrecognized filter type");
	}


	CDXLNode *filter_dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	CDXLNode *cond_node = CreateScalarCondFromQual(quals, var_col_id_mapping, has_distributed_tables);

	if (NULL != cond_node)
	{
		filter_dxlnode->AddChild(cond_node);
	}

	return filter_dxlnode;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateDXLNodeFromSublink
//
//	@doc:
//		Create a DXL node from a GPDB sublink node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateDXLNodeFromSublink
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	const SubLink *sublink = (SubLink *) expr;

	switch (sublink->subLinkType)
	{
		case EXPR_SUBLINK:
			return CreateScalarSubqueryFromSublink(sublink, var_col_id_mapping);

		case ALL_SUBLINK:
		case ANY_SUBLINK:
			return CreateQuantifiedSubqueryFromSublink(sublink, var_col_id_mapping);

		case EXISTS_SUBLINK:
			return CreateExistSubqueryFromSublink(sublink, var_col_id_mapping);

		default:
			{
				GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Non-Scalar Subquery"));
				return NULL;
			}

	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateQuantifiedSubqueryFromSublink
//
//	@doc:
//		Create ANY / ALL quantified sub query from a GPDB sublink node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateQuantifiedSubqueryFromSublink
	(
	const SubLink *sublink,
	const CMappingVarColId* var_col_id_mapping
	)
{
	CMappingVarColId *var_col_id_map_copy = var_col_id_mapping->CopyMapColId(m_memory_pool);

	CAutoP<CTranslatorQueryToDXL> query_to_dxl_translator;
	query_to_dxl_translator = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_memory_pool,
							m_md_accessor,
							m_col_id_generator,
							m_cte_id_generator,
							var_col_id_map_copy,
							(Query *) sublink->subselect,
							m_query_level + 1,
							m_cte_entries
							);

	CDXLNode *pdxlnInner = query_to_dxl_translator->PdxlnFromQueryInternal();

	DXLNodeArray *query_output_dxlnode_array = query_to_dxl_translator->PdrgpdxlnQueryOutput();
	DXLNodeArray *cte_dxlnode_array = query_to_dxl_translator->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_cte_producers, cte_dxlnode_array);

	if (1 != query_output_dxlnode_array->Size())
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature, GPOS_WSZ_LIT("Non-Scalar Subquery"));
	}

	m_has_distributed_tables = m_has_distributed_tables || query_to_dxl_translator->FHasDistributedTables();

	CDXLNode *dxl_sc_ident = (*query_output_dxlnode_array)[0];
	GPOS_ASSERT(NULL != dxl_sc_ident);

	// get dxl scalar identifier
	CDXLScalarIdent *scalar_ident = dynamic_cast<CDXLScalarIdent *>(dxl_sc_ident->GetOperator());

	// get the dxl column reference
	const CDXLColRef *dxl_colref = scalar_ident->MakeDXLColRef();
	const ULONG col_id = dxl_colref->Id();

	// get the test expression
	GPOS_ASSERT(IsA(sublink->testexpr, OpExpr));
	OpExpr *op_expr = (OpExpr*) sublink->testexpr;

	IMDId *mdid = GPOS_NEW(m_memory_pool) CMDIdGPDB(op_expr->opno);

	// get operator name
	const CWStringConst *str = GetDXLArrayCmpType(mdid);

	// translate left hand side of the expression
	GPOS_ASSERT(NULL != op_expr->args);
	Expr* pexprLHS = (Expr*) gpdb::ListNth(op_expr->args, 0);

	CDXLNode *pdxlnOuter = CreateScalarOpFromExpr(pexprLHS, var_col_id_mapping);

	CDXLNode *dxlnode = NULL;
	CDXLScalar *pdxlopSubquery = NULL;

	GPOS_ASSERT(ALL_SUBLINK == sublink->subLinkType || ANY_SUBLINK == sublink->subLinkType);
	if (ALL_SUBLINK == sublink->subLinkType)
	{
		pdxlopSubquery = GPOS_NEW(m_memory_pool) CDXLScalarSubqueryAll
								(
								m_memory_pool,
								mdid,
								GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, str),
								col_id
								);

	}
	else
	{
		pdxlopSubquery = GPOS_NEW(m_memory_pool) CDXLScalarSubqueryAny
								(
								m_memory_pool,
								mdid,
								GPOS_NEW(m_memory_pool) CMDName(m_memory_pool, str),
								col_id
								);
	}

	dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, pdxlopSubquery);

	dxlnode->AddChild(pdxlnOuter);
	dxlnode->AddChild(pdxlnInner);

#ifdef GPOS_DEBUG
	dxlnode->GetOperator()->AssertValid(dxlnode, false /* fValidateChildren */);
#endif

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateScalarSubqueryFromSublink
//
//	@doc:
//		Create a scalar subquery from a GPDB sublink node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateScalarSubqueryFromSublink
	(
	const SubLink *sublink,
	const CMappingVarColId *var_col_id_mapping
	)
{
	CMappingVarColId *var_col_id_map_copy = var_col_id_mapping->CopyMapColId(m_memory_pool);

	Query *pquerySublink = (Query *) sublink->subselect;
	CAutoP<CTranslatorQueryToDXL> query_to_dxl_translator;
	query_to_dxl_translator = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_memory_pool,
							m_md_accessor,
							m_col_id_generator,
							m_cte_id_generator,
							var_col_id_map_copy,
							pquerySublink,
							m_query_level + 1,
							m_cte_entries
							);
	CDXLNode *pdxlnSubQuery = query_to_dxl_translator->PdxlnFromQueryInternal();

	DXLNodeArray *query_output_dxlnode_array = query_to_dxl_translator->PdrgpdxlnQueryOutput();

	GPOS_ASSERT(1 == query_output_dxlnode_array->Size());

	DXLNodeArray *cte_dxlnode_array = query_to_dxl_translator->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_cte_producers, cte_dxlnode_array);
	m_has_distributed_tables = m_has_distributed_tables || query_to_dxl_translator->FHasDistributedTables();

	// get dxl scalar identifier
	CDXLNode *dxl_sc_ident = (*query_output_dxlnode_array)[0];
	GPOS_ASSERT(NULL != dxl_sc_ident);

	CDXLScalarIdent *scalar_ident = CDXLScalarIdent::Cast(dxl_sc_ident->GetOperator());

	// get the dxl column reference
	const CDXLColRef *dxl_colref = scalar_ident->MakeDXLColRef();
	const ULONG col_id = dxl_colref->Id();

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSubquery(m_memory_pool, col_id));

	dxlnode->AddChild(pdxlnSubQuery);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::TranslateArray
//
//	@doc:
//		Translate array
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::TranslateArray
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, ArrayExpr));

	const ArrayExpr *parrayexpr = (ArrayExpr *) expr;

	CDXLScalarArray *dxlop =
			GPOS_NEW(m_memory_pool) CDXLScalarArray
						(
						m_memory_pool,
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayexpr->element_typeid),
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayexpr->array_typeid),
						parrayexpr->multidims
						);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	TranslateScalarChildren(dxlnode, parrayexpr->elements, var_col_id_mapping);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::TranslateArrayRef
//
//	@doc:
//		Translate arrayref
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::TranslateArrayRef
	(
	const Expr *expr,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(IsA(expr, ArrayRef));

	const ArrayRef *parrayref = (ArrayRef *) expr;
	Oid restype;

	INT type_modifier = parrayref->reftypmod;
	/* slice and/or store operations yield the array type */
	if (parrayref->reflowerindexpr || parrayref->refassgnexpr)
		restype = parrayref->refarraytype;
	else
		restype = parrayref->refelemtype;

	CDXLScalarArrayRef *dxlop =
			GPOS_NEW(m_memory_pool) CDXLScalarArrayRef
						(
						m_memory_pool,
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayref->refelemtype),
						type_modifier,
						GPOS_NEW(m_memory_pool) CMDIdGPDB(parrayref->refarraytype),
						GPOS_NEW(m_memory_pool) CMDIdGPDB(restype)
						);

	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxlop);

	// add children
	AddArrayIndexList(dxlnode, parrayref->reflowerindexpr, CDXLScalarArrayRefIndexList::EilbLower, var_col_id_mapping);
	AddArrayIndexList(dxlnode, parrayref->refupperindexpr, CDXLScalarArrayRefIndexList::EilbUpper, var_col_id_mapping);

	dxlnode->AddChild(CreateScalarOpFromExpr(parrayref->refexpr, var_col_id_mapping));

	if (NULL != parrayref->refassgnexpr)
	{
		dxlnode->AddChild(CreateScalarOpFromExpr(parrayref->refassgnexpr, var_col_id_mapping));
	}

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::AddArrayIndexList
//
//	@doc:
//		Add an indexlist to the given DXL arrayref node
//
//---------------------------------------------------------------------------
void
CTranslatorScalarToDXL::AddArrayIndexList
	(
	CDXLNode *dxlnode,
	List *list,
	CDXLScalarArrayRefIndexList::EIndexListBound index_list_bound,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(EdxlopScalarArrayRef == dxlnode->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(CDXLScalarArrayRefIndexList::EilbSentinel > index_list_bound);

	CDXLNode *pdxlnIndexList =
			GPOS_NEW(m_memory_pool) CDXLNode
					(
					m_memory_pool,
					GPOS_NEW(m_memory_pool) CDXLScalarArrayRefIndexList(m_memory_pool, index_list_bound)
					);

	TranslateScalarChildren(pdxlnIndexList, list, var_col_id_mapping);
	dxlnode->AddChild(pdxlnIndexList);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDXLArrayCmpType
//
//	@doc:
//		Get the operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CTranslatorScalarToDXL::GetDXLArrayCmpType
	(
	IMDId *mdid
	)
	const
{
	// get operator name
	const IMDScalarOp *md_scalar_op = m_md_accessor->Pmdscop(mdid);

	const CWStringConst *str = md_scalar_op->Mdname().GetMDName();

	return str;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateExistSubqueryFromSublink
//
//	@doc:
//		Create a DXL EXISTS subquery node from the respective GPDB
//		sublink node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorScalarToDXL::CreateExistSubqueryFromSublink
	(
	const SubLink *sublink,
	const CMappingVarColId* var_col_id_mapping
	)
{
	GPOS_ASSERT(NULL != sublink);
	CMappingVarColId *var_col_id_map_copy = var_col_id_mapping->CopyMapColId(m_memory_pool);

	CAutoP<CTranslatorQueryToDXL> query_to_dxl_translator;
	query_to_dxl_translator = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							m_memory_pool,
							m_md_accessor,
							m_col_id_generator,
							m_cte_id_generator,
							var_col_id_map_copy,
							(Query *) sublink->subselect,
							m_query_level + 1,
							m_cte_entries
							);
	CDXLNode *root_dxl_node = query_to_dxl_translator->PdxlnFromQueryInternal();
	
	DXLNodeArray *cte_dxlnode_array = query_to_dxl_translator->PdrgpdxlnCTE();
	CUtils::AddRefAppend(m_cte_producers, cte_dxlnode_array);
	m_has_distributed_tables = m_has_distributed_tables || query_to_dxl_translator->FHasDistributedTables();
	
	CDXLNode *dxlnode = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarSubqueryExists(m_memory_pool));
	dxlnode->AddChild(root_dxl_node);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDatum
//
//	@doc:
//		Create CDXLDatum from GPDB datum
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::GetDatumVal
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	INT type_modifier,
	BOOL is_null,
	ULONG len,
	Datum datum
	)
{
	static const SDXLDatumTranslatorElem translators[] =
		{
			{IMDType::EtiInt2  , &CTranslatorScalarToDXL::PdxldatumInt2},
			{IMDType::EtiInt4  , &CTranslatorScalarToDXL::PdxldatumInt4},
			{IMDType::EtiInt8 , &CTranslatorScalarToDXL::PdxldatumInt8},
			{IMDType::EtiBool , &CTranslatorScalarToDXL::CreateBoolCDXLDatum},
			{IMDType::EtiOid  , &CTranslatorScalarToDXL::CreateOidCDXLDatum},
		};

	const ULONG num_translators = GPOS_ARRAY_SIZE(translators);
	// find translator for the datum type
	DxlDatumFromDatum *func_ptr = NULL;
	for (ULONG ul = 0; ul < num_translators; ul++)
	{
		SDXLDatumTranslatorElem elem = translators[ul];
		if (md_type->GetDatumType() == elem.type_info)
		{
			func_ptr = elem.func_ptr;
			break;
		}
	}

	if (NULL == func_ptr)
	{
		// generate a datum of generic type
		return CreateGenericCDXLDatum(memory_pool, md_type, type_modifier, is_null, len, datum);
	}
	else
	{
		return (*func_ptr)(memory_pool, md_type, is_null, len, datum);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateGenericCDXLDatum
//
//	@doc:
//		Translate a datum of generic type
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::CreateGenericCDXLDatum
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	INT type_modifier,
	BOOL is_null,
	ULONG len,
	Datum datum
	)
{
	CMDIdGPDB *mdid_old = CMDIdGPDB::CastMdid(md_type->MDId());
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(*mdid_old);

	BOOL is_const_by_val = md_type->IsPassedByValue();
	BYTE *bytes = GetByteArray(memory_pool, md_type, is_null, len, datum);
	ULONG length = 0;
	if (!is_null)
	{
		length = (ULONG) gpdb::DatumSize(datum, md_type->IsPassedByValue(), len);
	}

	CDouble double_value(0);
	if (CMDTypeGenericGPDB::HasByte2DoubleMapping(mdid))
	{
		double_value = GetDoubleValue(mdid, is_null, bytes, datum);
	}

	LINT lint_value = 0;
	if (CMDTypeGenericGPDB::HasByte2IntMapping(mdid))
	{
		lint_value = GetLintValue(mdid, is_null, bytes, length);
	}

	return CMDTypeGenericGPDB::CreateDXLDatumVal(memory_pool, mdid, type_modifier, is_const_by_val, is_null, bytes, length, lint_value, double_value);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateBoolCDXLDatum
//
//	@doc:
//		Translate a datum of type bool
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::CreateBoolCDXLDatum
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	BOOL is_null,
	ULONG , //len,
	Datum datum
	)
{
	GPOS_ASSERT(md_type->IsPassedByValue());
	CMDIdGPDB *mdid_old = CMDIdGPDB::CastMdid(md_type->MDId());
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(*mdid_old);

	return GPOS_NEW(memory_pool) CDXLDatumBool(memory_pool, mdid, is_null, gpdb::BoolFromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::CreateOidCDXLDatum
//
//	@doc:
//		Translate a datum of type oid
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::CreateOidCDXLDatum
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	BOOL is_null,
	ULONG , //len,
	Datum datum
	)
{
	GPOS_ASSERT(md_type->IsPassedByValue());
	CMDIdGPDB *mdid_old = CMDIdGPDB::CastMdid(md_type->MDId());
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(*mdid_old);

	return GPOS_NEW(memory_pool) CDXLDatumOid(memory_pool, mdid, is_null, gpdb::OidFromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumInt2
//
//	@doc:
//		Translate a datum of type int2
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumInt2
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	BOOL is_null,
	ULONG , //len,
	Datum datum
	)
{
	GPOS_ASSERT(md_type->IsPassedByValue());
	CMDIdGPDB *mdid_old = CMDIdGPDB::CastMdid(md_type->MDId());
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(*mdid_old);

	return GPOS_NEW(memory_pool) CDXLDatumInt2(memory_pool, mdid, is_null, gpdb::Int16FromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumInt4
//
//	@doc:
//		Translate a datum of type int4
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumInt4
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	BOOL is_null,
	ULONG , //len,
	Datum datum
	)
{
	GPOS_ASSERT(md_type->IsPassedByValue());
	CMDIdGPDB *mdid_old = CMDIdGPDB::CastMdid(md_type->MDId());
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(*mdid_old);

	return GPOS_NEW(memory_pool) CDXLDatumInt4(memory_pool, mdid, is_null, gpdb::Int32FromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::PdxldatumInt8
//
//	@doc:
//		Translate a datum of type int8
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorScalarToDXL::PdxldatumInt8
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	BOOL is_null,
	ULONG , //len,
	Datum datum
	)
{
	GPOS_ASSERT(md_type->IsPassedByValue());
	CMDIdGPDB *mdid_old = CMDIdGPDB::CastMdid(md_type->MDId());
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(*mdid_old);

	return GPOS_NEW(memory_pool) CDXLDatumInt8(memory_pool, mdid, is_null, gpdb::Int64FromDatum(datum));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDoubleValue
//
//	@doc:
//		Extract the double value of the datum
//---------------------------------------------------------------------------
CDouble
CTranslatorScalarToDXL::GetDoubleValue
	(
	IMDId *mdid,
	BOOL is_null,
	BYTE *bytes,
	Datum datum
	)
{
	GPOS_ASSERT(CMDTypeGenericGPDB::HasByte2DoubleMapping(mdid));

	double d = 0;

	if (is_null)
	{
		return CDouble(d);
	}

	if (mdid->Equals(&CMDIdGPDB::m_mdid_numeric))
	{
		Numeric num = (Numeric) (bytes);

		// NOTE: we assume that numeric_is_nan() cannot throw an error!
		if (numeric_is_nan(num))
		{
			// in GPDB NaN is considered the largest numeric number.
			return CDouble(GPOS_FP_ABS_MAX);
		}

		d = gpdb::NumericToDoubleNoOverflow(num);
	}
	else if (mdid->Equals(&CMDIdGPDB::m_mdid_float4))
	{
		float4 f = gpdb::Float4FromDatum(datum);

		if (isnan(f))
		{
			d = GPOS_FP_ABS_MAX;
		}
		else
		{
			d = (double) f;
		}
	}
	else if (mdid->Equals(&CMDIdGPDB::m_mdid_float8))
	{
		d = gpdb::Float8FromDatum(datum);

		if (isnan(d))
		{
			d = GPOS_FP_ABS_MAX;
		}
	}
	else if (CMDTypeGenericGPDB::IsTimeRelatedType(mdid))
	{
		d = gpdb::ConvertTimeValueToScalar(datum, CMDIdGPDB::CastMdid(mdid)->OidObjectId());
	}
	else if (CMDTypeGenericGPDB::IsNetworkRelatedType(mdid))
	{
		d = gpdb::ConvertNetworkToScalar(datum, CMDIdGPDB::CastMdid(mdid)->OidObjectId());
	}

	return CDouble(d);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetByteArray
//
//	@doc:
//		Extract the byte array value of the datum. The result is NULL if datum is NULL
//---------------------------------------------------------------------------
BYTE *
CTranslatorScalarToDXL::GetByteArray
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	BOOL is_null,
	ULONG len,
	Datum datum
	)
{
	ULONG length = 0;
	BYTE *bytes = NULL;

	if (is_null)
	{
		return bytes;
	}

	length = (ULONG) gpdb::DatumSize(datum, md_type->IsPassedByValue(), len);
	GPOS_ASSERT(length > 0);

	bytes = GPOS_NEW_ARRAY(memory_pool, BYTE, length);

	if (md_type->IsPassedByValue())
	{
		GPOS_ASSERT(length <= ULONG(sizeof(Datum)));
		clib::Memcpy(bytes, &datum, length);
	}
	else
	{
		clib::Memcpy(bytes, gpdb::PointerFromDatum(datum), length);
	}

	return bytes;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetLintValue
//
//	@doc:
//		Extract the long int value of a datum
//---------------------------------------------------------------------------
LINT
CTranslatorScalarToDXL::GetLintValue
	(
	IMDId *mdid,
	BOOL is_null,
	BYTE *bytes,
	ULONG length
	)
{
	GPOS_ASSERT(CMDTypeGenericGPDB::HasByte2IntMapping(mdid));

	LINT lint_value = 0;
	if (is_null)
	{
		return lint_value;
	}

	if (mdid->Equals(&CMDIdGPDB::m_mdid_cash))
	{
		// cash is a pass-by-ref type
		Datum datumConstVal = (Datum) 0;
		clib::Memcpy(&datumConstVal, bytes, length);
		// Date is internally represented as an int32
		lint_value = (LINT) (gpdb::Int32FromDatum(datumConstVal));

	}
	else
	{
		// use hash value
		ULONG hash = 0;
		if (is_null)
		{
			hash = gpos::HashValue<ULONG>(&hash);
		}
		else
		{
			hash = gpos::HashValue<BYTE>(bytes);
			for (ULONG ul = 1; ul < length; ul++)
			{
				hash = gpos::CombineHashes(hash, gpos::HashValue<BYTE>(&bytes[ul]));
			}
		}

		lint_value = (LINT) (hash / 4);
	}

	return lint_value;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorScalarToDXL::GetDatum
//
//	@doc:
//		Create IDatum from GPDB datum
//---------------------------------------------------------------------------
IDatum *
CTranslatorScalarToDXL::GetDatum
	(
	IMemoryPool *memory_pool,
	const IMDType *md_type,
	BOOL is_null,
	Datum gpdb_datum
	)
{
	ULONG length = md_type->Length();
	if (!md_type->IsPassedByValue() && !is_null)
	{
		INT len = dynamic_cast<const CMDTypeGenericGPDB *>(md_type)->GetGPDBLength();
		length = (ULONG) gpdb::DatumSize(gpdb_datum, md_type->IsPassedByValue(), len);
	}
	GPOS_ASSERT(is_null || length > 0);

	CDXLDatum *datum_dxl = CTranslatorScalarToDXL::GetDatumVal(memory_pool, md_type, gpmd::default_type_modifier, is_null, length, gpdb_datum);
	IDatum *datum = md_type->GetDatumForDXLDatum(memory_pool, datum_dxl);
	datum_dxl->Release();
	return datum;
}

// EOF
