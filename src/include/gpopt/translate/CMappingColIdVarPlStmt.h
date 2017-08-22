//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
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

			// translator context used to translate initplan and subplans associated
			// with a param node
			CContextDXLToPlStmt *m_pctxdxltoplstmt;

		public:

			CMappingColIdVarPlStmt
				(
				IMemoryPool *pmp,
				const CDXLTranslateContextBaseTable *pdxltrctxbt,
				DrgPdxltrctx *pdrgpdxltrctx,
				CDXLTranslateContext *pdxltrctxOut,
				CContextDXLToPlStmt *pctxdxltoplstmt
				);

			// translate DXL ScalarIdent node into GPDB Var node
			virtual
			Var *PvarFromDXLNodeScId(const CDXLScalarIdent *pdxlop);

			// translate DXL ScalarIdent node into GPDB Param node
			Param *PparamFromDXLNodeScId(const CDXLScalarIdent *pdxlop);

			// get the output translator context
			CDXLTranslateContext *PpdxltrctxOut();

			// return the context of the DXL->PlStmt translation
			CContextDXLToPlStmt *Pctxdxltoplstmt();
	};
}

#endif // GPDXL_CMappingColIdVarPlStmt_H

// EOF
