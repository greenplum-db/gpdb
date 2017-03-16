//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CMappingColIdVarPlStmt.h
//
//	@doc:
//		Class defining the functions that provide the mapping between Var, Param
//		and variables of Sub-query to CDXLNode during Query->DXL translation
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CMappingColIdVarPlStmt_H
#define GPDXL_CMappingColIdVarPlStmt_H

#include "gpos/base.h"
#include "gpos/common/CHashMap.h"
#include "gpos/common/CDynamicPtrArray.h"


#include "gpopt/translate/CMappingColIdVar.h"
#include "gpopt/translate/CDXLTranslateContext.h"
#include "gpopt/translate/CTranslatorUtils.h"

//fwd decl
struct Var;
struct Plan;

namespace gpdxl
{
	// fwd decl
	class CDXLTranslateContextBaseTable;
	class CContextDXLToPlStmt;

	//---------------------------------------------------------------------------
	//	@class:
	//		CMappingColIdVarPlStmt
	//
	//	@doc:
	//	Class defining functions that provide the mapping between Var, Param
	//	and variables of Sub-query to CDXLNode during Query->DXL translation
	//
	//---------------------------------------------------------------------------
	class CMappingColIdVarPlStmt : public CMappingColIdVar
	{
		private:

			const CDXLTranslateContextBaseTable *m_pdxltrctxbt;

			// the array of translator context (one for each child of the DXL operator)
			DrgPdxltrctx *m_pdrgpdxltrctx;

			CDXLTranslateContext *m_pdxltrctxOut;

			Plan *m_pplan;

			// translator context used to translate initplan and subplans associated
			// with a param node
			CContextDXLToPlStmt *m_pctxdxltoplstmt;

			BOOL m_fUseInnerOuter;

			// map of UlColId to RTE index for printable filters
			HMUlUl *m_phmColIdRteIdxPrintableFilter;

			// map UlColdId to Attno for printable filters
			HMUlUl *m_phmColIdAttnoPrintableFilter;

			// map UlColdId to Column Alias for printable filters
			typedef CHashMap<ULONG, CWStringConst, gpos::UlHash<ULONG>, gpos::FEqual<ULONG>,
				CleanupDelete<ULONG>, CleanupDelete<CWStringConst> > HMUlStr;

			HMUlStr *m_phmColIdAliasPrintableFilter;

		public:

			CMappingColIdVarPlStmt
				(
				IMemoryPool *pmp,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				CDXLTranslateContext *pdxltrctxOut,
				CContextDXLToPlStmt *pctxdxltoplstmt,
				Plan *pplan
				);

			CMappingColIdVarPlStmt
				(
				IMemoryPool *pmp,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				CDXLTranslateContext *pdxltrctxOut,
				CContextDXLToPlStmt *pctxdxltoplstmt,
				Plan *pplan,
				bool fUseInnerOuter,
				HMUlUl *phmColIdRteIdxPrintableFilter,
				HMUlUl *phmColIdAttnoPrintableFilter,
				HMUlStr *phmColIdAliasPrintableFilter
				);

			// translate DXL ScalarIdent node into GPDB Var node
			virtual
			Var *PvarFromDXLNodeScId(const CDXLScalarIdent *pdxlop);

			// translate DXL ScalarIdent node into GPDB Param node
			Param *PparamFromDXLNodeScId(const CDXLScalarIdent *pdxlop);

			// get the output translator context
			CDXLTranslateContext *PpdxltrctxOut();

			// return the parent plan
			Plan *Pplan();

			// return the context of the DXL->PlStmt translation
			CContextDXLToPlStmt *Pctxdxltoplstmt();

			BOOL FuseInnerOuter();

			HMUlUl *PhmColIdRteIdxPrintableFilter();

			HMUlUl *PhmColIdAttnoPrintableFilter();

			HMUlStr *PhmColIdAliasPrintableFilter();
	};
}

#endif // GPDXL_CMappingColIdVarPlStmt_H

// EOF
