//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CTranslatorDXLToScalar.h
//
//	@doc:
//		Class providing methods for translating from DXL Scalar Node to
//		GPDB's Expr.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorDXLToScalar_H
#define GPDXL_CTranslatorDXLToScalar_H


#include "gpopt/translate/CMappingColIdVar.h"
#include "gpopt/translate/CContextDXLToPlStmt.h"
#include "gpopt/translate/CMappingElementColIdParamId.h"

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLDatum.h"
#include "naucrates/dxl/operators/CDXLScalarArrayRefIndexList.h"
#include "naucrates/dxl/operators/CDXLScalarCast.h"

// fwd declarations
namespace gpopt
{
	class CMDAccessor;
}

namespace gpmd
{
	class IMDId;
}

struct Aggref;
struct BoolExpr;
struct BooleanTest;
struct CaseExpr;
struct Expr;
struct FuncExpr;
struct NullTest;
struct OpExpr;
struct Param;
struct Plan;
struct RelabelType;
struct ScalarArrayOpExpr;
struct Const;
struct List;
struct SubLink;
struct SubPlan;

namespace gpdxl
{
	using namespace gpopt;
	using namespace gpmd;

	//---------------------------------------------------------------------------
	//	@class:
	//		CTranslatorDXLToScalar
	//
	//	@doc:
	//		Class providing methods for translating from DXL Scalar Node to
	//		GPDB's Expr.
	//
	//---------------------------------------------------------------------------
	class CTranslatorDXLToScalar
	{
		// shorthand for functions for translating DXL nodes to GPDB expressions
		typedef Expr * (CTranslatorDXLToScalar::*expr_func_ptr)(const CDXLNode *dxlnode, CMappingColIdVar *col_id_var);

		private:

			// pair of DXL op id and translator function
			struct STranslatorElem
			{
				Edxlopid eopid;
				expr_func_ptr translate_func;
			};

			// shorthand for functions for translating DXL nodes to GPDB expressions
			typedef Const * (CTranslatorDXLToScalar::*const_func_ptr)(CDXLDatum *);

			// pair of DXL datum type and translator function
			struct SDatumTranslatorElem
			{
				CDXLDatum::EdxldatumType edxldt;
				const_func_ptr translate_func;
			};

			IMemoryPool *m_memory_pool;

			// meta data accessor
			CMDAccessor *m_md_accessor;

			// The parent plan needed when translating an initplan
			Plan *m_plan;

			// indicates whether a sublink was encountered during translation of the scalar subtree
			BOOL m_has_subqueries;
			
			// number of segments
			ULONG m_num_of_segments; 

			// translate a CDXLScalarArrayComp into a GPDB ScalarArrayOpExpr
			Expr *CreateScalarArrayCompFromDXLNode
				(
				const CDXLNode *scalar_array_cmp_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarOpExprFromDXL
				(
				const CDXLNode *scalar_op_expr_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarDistinctCmpExprFromDXL
				(
				const CDXLNode *scalar_distinct_cmp_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarBoolExprFromDXL
				(
				const CDXLNode *scalar_bool_expr_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarBoolTestExprFromDXL
				(
				const CDXLNode *scalar_boolean_test_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarCastExprFromDXL
				(
				const CDXLNode *scalar_relabel_type_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarCoerceToDomainExprFromDXL
				(
				const CDXLNode *coerce_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarCoerceViaIOExprFromDXL
				(
				const CDXLNode *coerce_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarArrayCoerceExprFromDXL
				(
				const CDXLNode *coerce_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarNULLTestExprFromDXL
				(
				const CDXLNode *scalar_null_test_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarNULLIfExprFromDXL
				(
				const CDXLNode *scalar_null_if_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarIfStmtExprFromDXL
				(
				const CDXLNode *scalar_if_stmt_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarSwitchExprFromDXL
				(
				const CDXLNode *scalar_switch_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarCaseTestExprFromDXL
				(
				const CDXLNode *scalar_case_test_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarAggrefExprFromDXL
				(
				const CDXLNode *aggref_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarWindowRefExprFromDXL
				(
				const CDXLNode *scalar_winref_node,
				CMappingColIdVar *col_id_var
				);

			Expr *CreateScalarFuncExprFromDXL
				(
				const CDXLNode *scalar_func_expr_node,
				CMappingColIdVar *col_id_var
				);

			// return a GPDB subplan from a DXL subplan
			Expr *CreateScalarSubplanExprFromDXL
				(
				const CDXLNode *scalar_sub_plan_node,
				CMappingColIdVar *col_id_var
				);
			
			// build subplan node
			SubPlan *CreateSubplanFromChildPlan
				(
				Plan *plan_child,
				SubLinkType slink,
				CContextDXLToPlStmt *dxl_to_plstmt_ctxt
				);

			// translate subplan test expression
			Expr *CreateSubplanTestExprFromDXL
				(
				CDXLNode *test_expr_node,
				SubLinkType slink,
				CMappingColIdVar *col_id_var,
				List **param_ids_list
				);
			
			// translate subplan parameters
			void TranslateSubplanParams
        			(
        			SubPlan *sub_plan,
        			CDXLTranslateContext *dxl_translator_ctxt,
        			const DrgPdxlcr *outer_refs,
				CMappingColIdVar *col_id_var
       	 			);

			CHAR *GetSubplanAlias(ULONG plan_id);

			Param *CreateParamFromMapping
				(
				const CMappingElementColIdParamId *col_id_to_param_id_map
				);

			// translate a scalar coalesce
			Expr *CreateScalarCoalesceExprFromDXL
				(
				const CDXLNode *scalar_coalesce_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar minmax
			Expr *CreateScalarMinMaxExprFromDXL
				(
				const CDXLNode *scalar_min_max_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scconstval
			Expr *CreateScalarConstExprFromDXL
				(
				const CDXLNode *scalar_const_node,
				CMappingColIdVar *col_id_var
				);

			// translate an array expression
			Expr *CreateArrayExprFromDXL
				(
				const CDXLNode *scalar_array_node,
				CMappingColIdVar *col_id_var
				);

			// translate an arrayref expression
			Expr *CreateArrayRefExprFromDXL
				(
				const CDXLNode *scalar_array_ref_node,
				CMappingColIdVar *col_id_var
				);

			// translate an arrayref index list
			List *CreateArrayRefIndexListExprFromDXL
				(
				const CDXLNode *index_list_node,
				CDXLScalarArrayRefIndexList::EIndexListBound index_list_bound,
				CMappingColIdVar *col_id_var
				);

			// translate a DML action expression
			Expr *CreateDMLActionExprFromDXL
				(
				const CDXLNode *dml_action_node,
				CMappingColIdVar *col_id_var
				);
			
			
			// translate children of DXL node, and add them to list
			List *PlistTranslateScalarChildren
				(
				List *list,
				const CDXLNode *dxlnode,
				CMappingColIdVar *col_id_var
				);

			// return the operator return type oid for the given func id.
			OID GetFunctionReturnTypeOid(IMDId *mdid) const;

			// translate dxldatum to GPDB Const
			Const *ConvertDXLDatumToConstOid(CDXLDatum *datum_dxl);
			Const *ConvertDXLDatumToConstInt2(CDXLDatum *datum_dxl);
			Const *ConvertDXLDatumToConstInt4(CDXLDatum *datum_dxl);
			Const *ConvertDXLDatumToConstInt8(CDXLDatum *datum_dxl);
			Const *ConvertDXLDatumToConstBool(CDXLDatum *datum_dxl);
			Const *CreateConstGenericExprFromDXL(CDXLDatum *datum_dxl);
			Expr *CreateRelabelTypeOrFuncExprFromDXL
				(
				const CDXLScalarCast *scalar_cast,
				Expr *pexprChild
				);

			// private copy ctor
			CTranslatorDXLToScalar(const CTranslatorDXLToScalar&);

		public:
			struct STypeOidAndTypeModifier
			{
				OID oid_type;
				INT type_modifier;
			};

			// ctor
			CTranslatorDXLToScalar(IMemoryPool *memory_pool, CMDAccessor *md_accessor, ULONG num_segments);

			// translate DXL scalar operator node into an Expr expression
			// This function is called during the translation of DXL->Query or DXL->Query
			Expr *CreateScalarExprFromDXL
				(
				const CDXLNode *scalar_op_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar part default into an Expr
			Expr *CreatePartDefaultExprFromDXL
				(
				const CDXLNode *part_default_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar part bound into an Expr
			Expr *CreatePartBoundExprFromDXL
				(
				const CDXLNode *part_bound_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar part bound inclusion into an Expr
			Expr *CreatePartBoundInclusionExprFromDXL
				(
				const CDXLNode *part_bound_incl_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar part bound openness into an Expr
			Expr *CreatePartBoundOpenExprFromDXL
				(
				const CDXLNode *part_bound_open_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar part list values into an Expr
			Expr *CreatePartListValuesExprFromDXL
				(
				const CDXLNode *part_list_values_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar part list null test into an Expr
			Expr *CreatePartListNullTestExprFromDXL
				(
				const CDXLNode *part_list_null_test_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar ident into an Expr
			Expr *CreateScalarIdExprFromDXL
				(
				const CDXLNode *scalar_id_node,
				CMappingColIdVar *col_id_var
				);

			// translate a scalar comparison into an Expr
			Expr *CreateScalarCmpExprFromDXL
				(
				const CDXLNode *scalar_cmp_node,
				CMappingColIdVar *col_id_var
				);


			// checks if the operator return a boolean result
			static
			BOOL HasBoolResult(CDXLNode *dxlnode, CMDAccessor *md_accessor);

			// check if the operator is a "true" bool constant
			static
			BOOL HasConstTrue(CDXLNode *dxlnode, CMDAccessor *md_accessor);

			// check if the operator is a NULL constant
			static
			BOOL HasConstNull(CDXLNode *dxlnode);

			// are there subqueries in the tree
			BOOL HasSubqueries() const
			{
				return m_has_subqueries;
			}
			
			// translate a DXL datum into GPDB const expression
			Expr *CreateConstExprFromDXL(CDXLDatum *datum_dxl);
	};
}
#endif // !GPDXL_CTranslatorDXLToScalar_H

// EOF
