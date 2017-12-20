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
		typedef Expr * (CTranslatorDXLToScalar::*PfPexpr)(const CDXLNode *pdxln, CMappingColIdVar *pmapcidvar);

		private:

			// pair of DXL op id and translator function
			struct STranslatorElem
			{
				Edxlopid eopid;
				PfPexpr pf;
			};

			// shorthand for functions for translating DXL nodes to GPDB expressions
			typedef Const * (CTranslatorDXLToScalar::*PfPconst)(CDXLDatum *);

			// pair of DXL datum type and translator function
			struct SDatumTranslatorElem
			{
				CDXLDatum::EdxldatumType edxldt;
				PfPconst pf;
			};

			IMemoryPool *m_pmp;

			// meta data accessor
			CMDAccessor *m_pmda;

			// The parent plan needed when translating an initplan
			Plan *m_pplan;

			// indicates whether a sublink was encountered during translation of the scalar subtree
			BOOL m_fHasSubqueries;
			
			// number of segments
			ULONG m_ulSegments; 

			// translate a CDXLScalarArrayComp into a GPDB ScalarArrayOpExpr
			Expr *PstrarrayopexprFromDXLNodeScArrayComp
				(
				const CDXLNode *pdxlnScArrayComp,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PopexprFromDXLNodeScOpExpr
				(
				const CDXLNode *pdxlnScOpExpr,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PdistexprFromDXLNodeScDistinctComp
				(
				const CDXLNode *pdxlnScDistComp,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PboolexprFromDXLNodeScBoolExpr
				(
				const CDXLNode *pdxlnScBoolExpr,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PbooleantestFromDXLNodeScBooleanTest
				(
				const CDXLNode *pdxlnScBooleanTest,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PrelabeltypeFromDXLNodeScCast
				(
				const CDXLNode *pdxlnScRelabelType,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PcoerceFromDXLNodeScCoerceToDomain
				(
				const CDXLNode *pdxlnScCoerceToDomain,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PcoerceFromDXLNodeScCoerceViaIO
				(
				const CDXLNode *pdxlnScCoerceViaIO,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PcoerceFromDXLNodeScArrayCoerceExpr
				(
				const CDXLNode *pdxlnScArrayCoerceExpr,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PnulltestFromDXLNodeScNullTest
				(
				const CDXLNode *pdxlnScNullTest,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PnullifFromDXLNodeScNullIf
				(
				const CDXLNode *pdxlnScNullIf,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PcaseexprFromDXLNodeScIfStmt
				(
				const CDXLNode *pdxlnScCaseExpr,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PcaseexprFromDXLNodeScSwitch
				(
				const CDXLNode *pdxlnScSwitch,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PcasetestexprFromDXLNodeScCaseTest
				(
				const CDXLNode *pdxlnScSwitch,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PaggrefFromDXLNodeScAggref
				(
				const CDXLNode *pdxlnAggref,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PwindowrefFromDXLNodeScWindowRef
				(
				const CDXLNode *pdxlnAggref,
				CMappingColIdVar *pmapcidvar
				);

			Expr *PfuncexprFromDXLNodeScFuncExpr
				(
				const CDXLNode *pdxlnFuncExpr,
				CMappingColIdVar *pmapcidvar
				);

			// return a GPDB subplan from a DXL subplan
			Expr *PsubplanFromDXLNodeScSubPlan
				(
				const CDXLNode *pdxlnSubPlan,
				CMappingColIdVar *pmapcidvar
				);
			
			// build subplan node
			SubPlan *PsubplanFromChildPlan
				(
				Plan *pplanChild,
				SubLinkType slink,
				CContextDXLToPlStmt *pctxdxltoplstmt
				);

			// translate subplan test expression
			Expr *PexprSubplanTestExpr
				(
				CDXLNode *pdxlnTestExpr,
				SubLinkType slink,
				CMappingColIdVar *pmapcidvar,
				List **plparamIds
				);
			
			// translate subplan parameters
			void TranslateSubplanParams
        			(
        			SubPlan *psubplan,
        			CDXLTranslateContext *pdxltrctx,
        			const DrgPdxlcr *pdrgdxlcrOuterRefs,
				CMappingColIdVar *pmapcidvar
       	 			);

			CHAR *SzSubplanAlias(ULONG ulPlanId);

			Param *PparamFromMapping
				(
				const CMappingElementColIdParamId *pmecolidparamid
				);

			// translate a scalar coalesce
			Expr *PcoalesceFromDXLNodeScCoalesce
				(
				const CDXLNode *pdxlnScCoalesce,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar minmax
			Expr *PminmaxFromDXLNodeScMinMax
				(
				const CDXLNode *pdxlnScMinMax,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scconstval
			Expr *PconstFromDXLNodeScConst
				(
				const CDXLNode *pdxlnScConst,
				CMappingColIdVar *pmapcidvar
				);

			// translate an array expression
			Expr *PexprArray
				(
				const CDXLNode *pdxlnArray,
				CMappingColIdVar *pmapcidvar
				);

			// translate an arrayref expression
			Expr *PexprArrayRef
				(
				const CDXLNode *pdxlnArrayref,
				CMappingColIdVar *pmapcidvar
				);

			// translate an arrayref index list
			List *PlTranslateArrayRefIndexList
				(
				const CDXLNode *pdxlnIndexlist,
				CDXLScalarArrayRefIndexList::EIndexListBound eilb,
				CMappingColIdVar *pmapcidvar
				);

			// translate a DML action expression
			Expr *PexprDMLAction
				(
				const CDXLNode *pdxlnDMLAction,
				CMappingColIdVar *pmapcidvar
				);
			
			
			// translate children of DXL node, and add them to list
			List *PlistTranslateScalarChildren
				(
				List *plist,
				const CDXLNode *pdxln,
				CMappingColIdVar *pmapcidvar
				);

			// return the operator return type oid for the given func id.
			OID OidFunctionReturnType(IMDId *pmdid) const;

			// translate dxldatum to GPDB Const
			Const *PconstOid(CDXLDatum *pdxldatum);
			Const *PconstInt2(CDXLDatum *pdxldatum);
			Const *PconstInt4(CDXLDatum *pdxldatum);
			Const *PconstInt8(CDXLDatum *pdxldatum);
			Const *PconstBool(CDXLDatum *pdxldatum);
			Const *PconstGeneric(CDXLDatum *pdxldatum);
			Expr *PrelabeltypeOrFuncexprFromDXLNodeScalarCast
				(
				const CDXLScalarCast *pdxlscalarcast,
				Expr *pexprChild
				);

			// private copy ctor
			CTranslatorDXLToScalar(const CTranslatorDXLToScalar&);

		public:
			// ctor
			CTranslatorDXLToScalar(IMemoryPool *pmp, CMDAccessor *pmda, ULONG ulSegments);

			// translate DXL scalar operator node into an Expr expression
			// This function is called during the translation of DXL->Query or DXL->Query
			Expr *PexprFromDXLNodeScalar
				(
				const CDXLNode *pdxlnScOp,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar part default into an Expr
			Expr *PexprPartDefault
				(
				const CDXLNode *pdxlnPartDefault,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar part bound into an Expr
			Expr *PexprPartBound
				(
				const CDXLNode *pdxlnPartBound,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar part bound inclusion into an Expr
			Expr *PexprPartBoundInclusion
				(
				const CDXLNode *pdxlnPartBoundIncl,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar part bound openness into an Expr
			Expr *PexprPartBoundOpen
				(
				const CDXLNode *pdxlnPartBoundOpen,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar part list values into an Expr
			Expr *PexprPartListValues
				(
				const CDXLNode *pdxlnPartListValues,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar part list null test into an Expr
			Expr *PexprPartListNullTest
				(
				const CDXLNode *pdxlnPartListNullTest,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar ident into an Expr
			Expr *PexprFromDXLNodeScId
				(
				const CDXLNode *pdxlnScId,
				CMappingColIdVar *pmapcidvar
				);

			// translate a scalar comparison into an Expr
			Expr *PopexprFromDXLNodeScCmp
				(
				const CDXLNode *pdxlnScCmp,
				CMappingColIdVar *pmapcidvar
				);


			// checks if the operator return a boolean result
			static
			BOOL FBoolean(CDXLNode *pdxln, CMDAccessor *pmda);

			// check if the operator is a "true" bool constant
			static
			BOOL FConstTrue(CDXLNode *pdxln, CMDAccessor *pmda);

			// check if the operator is a NULL constant
			static
			BOOL FConstNull(CDXLNode *pdxln);

			// are there subqueries in the tree
			BOOL FHasSubqueries() const
			{
				return m_fHasSubqueries;
			}
			
			// translate a DXL datum into GPDB const expression
			Expr *PconstFromDXLDatum(CDXLDatum *pdxldatum);
	};
}
#endif // !GPDXL_CTranslatorDXLToScalar_H

// EOF
