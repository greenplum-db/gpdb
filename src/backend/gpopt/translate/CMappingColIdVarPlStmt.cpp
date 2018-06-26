//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMappingColIdVarPlStmt.cpp
//
//	@doc:
//		Implentation of the functions that provide the mapping between Var, Param
//		and variables of Sub-query to CDXLNode during Query->DXL translation
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"
#include "nodes/primnodes.h"

#include "gpopt/translate/CMappingColIdVarPlStmt.h"
#include "gpopt/translate/CDXLTranslateContextBaseTable.h"

#include "naucrates/exception.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/dxl/operators/CDXLScalarIdent.h"

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMappingColIdVarPlStmt::CMappingColIdVarPlStmt
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CMappingColIdVarPlStmt::CMappingColIdVarPlStmt
	(
	IMemoryPool *memory_pool,
	const CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctx,
	CDXLTranslateContext *pdxltrctxOut,
	CContextDXLToPlStmt *pctxdxltoplstmt
	)
	:
	CMappingColIdVar(memory_pool),
	m_pdxltrctxbt(pdxltrctxbt),
	m_pdrgpdxltrctx(pdrgpdxltrctx),
	m_pdxltrctxOut(pdxltrctxOut),
	m_pctxdxltoplstmt(pctxdxltoplstmt)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingColIdVarPlStmt::Pctxdxltoplstmt
//
//	@doc:
//		Returns the DXL->PlStmt translation context
//
//---------------------------------------------------------------------------
CContextDXLToPlStmt *
CMappingColIdVarPlStmt::Pctxdxltoplstmt()
{
	return m_pctxdxltoplstmt;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingColIdVarPlStmt::PpdxltrctxOut
//
//	@doc:
//		Returns the output translation context
//
//---------------------------------------------------------------------------
CDXLTranslateContext *
CMappingColIdVarPlStmt::PpdxltrctxOut()
{
	return m_pdxltrctxOut;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingColIdVarPlStmt::PparamFromDXLNodeScId
//
//	@doc:
//		Translates a DXL scalar identifier operator into a GPDB Param node
//
//---------------------------------------------------------------------------
Param *
CMappingColIdVarPlStmt::PparamFromDXLNodeScId
	(
	const CDXLScalarIdent *pdxlop
	)
{
	GPOS_ASSERT(NULL != m_pdxltrctxOut);

	Param *pparam = NULL;

	const ULONG col_id = pdxlop->MakeDXLColRef()->Id();
	const CMappingElementColIdParamId *pmecolidparamid = m_pdxltrctxOut->GetParamIdMappingElement(col_id);

	if (NULL != pmecolidparamid)
	{
		pparam = MakeNode(Param);
		pparam->paramkind = PARAM_EXEC;
		pparam->paramid = pmecolidparamid->UlParamId();
		pparam->paramtype = CMDIdGPDB::CastMdid(pmecolidparamid->MDIdType())->OidObjectId();
		pparam->paramtypmod = pmecolidparamid->TypeModifier();
	}

	return pparam;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingColIdVarPlStmt::PvarFromDXLNodeScId
//
//	@doc:
//		Translates a DXL scalar identifier operator into a GPDB Var node
//
//---------------------------------------------------------------------------
Var *
CMappingColIdVarPlStmt::PvarFromDXLNodeScId
	(
	const CDXLScalarIdent *pdxlop
	)
{
	Index idxVarno = 0;
	AttrNumber attno = 0;

	Index idxVarnoold = 0;
	AttrNumber attnoOld = 0;

	const ULONG col_id = pdxlop->MakeDXLColRef()->Id();
	if (NULL != m_pdxltrctxbt)
	{
		// scalar id is used in a base table operator node
		idxVarno = m_pdxltrctxbt->GetRelIndex();
		attno = (AttrNumber) m_pdxltrctxbt->GetAttnoForColId(col_id);

		idxVarnoold = idxVarno;
		attnoOld = attno;
	}

	// if lookup has failed in the first step, attempt lookup again using outer and inner contexts
	if (0 == attno && NULL != m_pdrgpdxltrctx)
	{
		GPOS_ASSERT(0 != m_pdrgpdxltrctx->Size());

		const CDXLTranslateContext *pdxltrctxLeft = (*m_pdrgpdxltrctx)[0];

		//	const CDXLTranslateContext *pdxltrctxRight

		// not a base table
		GPOS_ASSERT(NULL != pdxltrctxLeft);

		// lookup column in the left child translation context
		const TargetEntry *target_entry = pdxltrctxLeft->GetTargetEntry(col_id);

		if (NULL != target_entry)
		{
			// identifier comes from left child
			idxVarno = OUTER;
		}
		else
		{
			const ULONG ulContexts = m_pdrgpdxltrctx->Size();
			if (2 > ulContexts)
			{
				// there are no more children. col id not found in this tree
				// and must be an outer ref
				return NULL;
			}

			const CDXLTranslateContext *pdxltrctxRight = (*m_pdrgpdxltrctx)[1];

			// identifier must come from right child
			GPOS_ASSERT(NULL != pdxltrctxRight);

			target_entry = pdxltrctxRight->GetTargetEntry(col_id);

			idxVarno = INNER;

			// check any additional contexts if col is still not found yet
			for (ULONG ul = 2; NULL == target_entry && ul < ulContexts; ul++)
			{
				const CDXLTranslateContext *pdxltrctx = (*m_pdrgpdxltrctx)[ul];
				GPOS_ASSERT(NULL != pdxltrctx);

				target_entry = pdxltrctx->GetTargetEntry(col_id);
				if (NULL == target_entry)
				{
					continue;
				}

				Var *pv = (Var*) target_entry->expr;
				idxVarno = pv->varno;
			}
		}

		if (NULL  == target_entry)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, col_id);
		}

		attno = target_entry->resno;

		// find the original varno and attno for this column
		if (IsA(target_entry->expr, Var))
		{
			Var *pv = (Var*) target_entry->expr;
			idxVarnoold = pv->varnoold;
			attnoOld = pv->varoattno;
		}
		else
		{
			idxVarnoold = idxVarno;
			attnoOld = attno;
		}
	}

	Var *var = gpdb::PvarMakeVar
						(
						idxVarno,
						attno,
						CMDIdGPDB::CastMdid(pdxlop->MDIdType())->OidObjectId(),
						pdxlop->TypeModifier(),
						0	// varlevelsup
						);

	// set varnoold and varoattno since makeVar does not set them properly
	var->varnoold = idxVarnoold;
	var->varoattno = attnoOld;

	return var;
}

// EOF
