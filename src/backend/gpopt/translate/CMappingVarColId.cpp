//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMappingVarColId.cpp
//
//	@doc:
//		Implementation of base (abstract) var mapping class
//
//	@test:
//
//
//---------------------------------------------------------------------------
#include "postgres.h"
#include "gpopt/translate/CMappingVarColId.h"
#include "gpopt/translate/CTranslatorUtils.h"

#include "nodes/primnodes.h"
#include "nodes/value.h"


#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/md/IMDIndex.h"

#include "gpos/error/CAutoTrace.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::CMappingVarColId
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMappingVarColId::CMappingVarColId
	(
	IMemoryPool *pmp
	)
	:
	m_pmp(pmp)
{
	m_pmvcmap = GPOS_NEW(m_pmp) CMVCMap(m_pmp);
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::Pgpdbattoptcol
//
//	@doc:
//		Given a gpdb attribute, return the mapping info to opt col
//
//---------------------------------------------------------------------------
const CGPDBAttOptCol *
CMappingVarColId::Pgpdbattoptcol
	(
	ULONG ulCurrentQueryLevel,
	const Var *pvar,
	EPlStmtPhysicalOpType eplsphoptype
	)
	const
{
	GPOS_ASSERT(NULL != pvar);
	GPOS_ASSERT(ulCurrentQueryLevel >= pvar->varlevelsup);

	// absolute query level of var
	ULONG ulAbsQueryLevel = ulCurrentQueryLevel - pvar->varlevelsup;

	// extract varno
	ULONG ulVarNo = pvar->varno;
	if (EpspotWindow == eplsphoptype || EpspotAgg == eplsphoptype || EpspotMaterialize == eplsphoptype)
	{
		// Agg and Materialize need to employ OUTER, since they have other
		// values in GPDB world
		ulVarNo = OUTER_VAR;
	}

	CGPDBAttInfo *pgpdbattinfo = GPOS_NEW(m_pmp) CGPDBAttInfo(ulAbsQueryLevel, ulVarNo, pvar->varattno);
	CGPDBAttOptCol *pgpdbattoptcol = m_pmvcmap->PtLookup(pgpdbattinfo);
	
	if (NULL == pgpdbattoptcol)
	{
		// TODO: Sept 09 2013, remove temporary fix (revert exception to assert) to avoid crash during algebrization
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLError, GPOS_WSZ_LIT("No variable"));
	}

	pgpdbattinfo->Release();
	return pgpdbattoptcol;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::PstrColName
//
//	@doc:
//		Given a gpdb attribute, return a column name in optimizer world
//
//---------------------------------------------------------------------------
const CWStringBase *
CMappingVarColId::PstrColName
	(
	ULONG ulCurrentQueryLevel,
	const Var *pvar,
	EPlStmtPhysicalOpType eplsphoptype
	)
	const
{
	return Pgpdbattoptcol(ulCurrentQueryLevel, pvar, eplsphoptype)->Poptcolinfo()->PstrColName();
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::UlColId
//
//	@doc:
//		given a gpdb attribute, return a column id in optimizer world
//
//---------------------------------------------------------------------------
ULONG
CMappingVarColId::UlColId
	(
	ULONG ulCurrentQueryLevel,
	const Var *pvar,
	EPlStmtPhysicalOpType eplsphoptype
	)
	const
{
	return Pgpdbattoptcol(ulCurrentQueryLevel, pvar, eplsphoptype)->Poptcolinfo()->UlColId();
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::Insert
//
//	@doc:
//		Insert a single entry into the hash map
//
//---------------------------------------------------------------------------
void
CMappingVarColId::Insert
	(
	ULONG ulQueryLevel,
	ULONG ulVarNo,
	INT iAttNo,
	ULONG ulColId,
	CWStringBase *pstrColName
	)
{
	// GPDB agg node uses 0 in Var, but that should've been taken care of
	// by translator
	GPOS_ASSERT(ulVarNo > 0);

	// create key
	CGPDBAttInfo *pgpdbattinfo = GPOS_NEW(m_pmp) CGPDBAttInfo(ulQueryLevel, ulVarNo, iAttNo);

	// create value
	COptColInfo *poptcolinfo = GPOS_NEW(m_pmp) COptColInfo(ulColId, pstrColName);

	// key is part of value, bump up refcount
	pgpdbattinfo->AddRef();
	CGPDBAttOptCol *pgpdbattoptcol = GPOS_NEW(m_pmp) CGPDBAttOptCol(pgpdbattinfo, poptcolinfo);

#ifdef GPOS_DEBUG
	BOOL fResult =
#endif // GPOS_DEBUG
			m_pmvcmap->FInsert(pgpdbattinfo, pgpdbattoptcol);

	GPOS_ASSERT(fResult);
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadTblColumns
//
//	@doc:
//		Load up information from GPDB's base table RTE and corresponding
//		optimizer table descriptor
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadTblColumns
	(
	ULONG ulQueryLevel,
	ULONG ulRTEIndex,
	const CDXLTableDescr *pdxltabdesc
	)
{
	GPOS_ASSERT(NULL != pdxltabdesc);
	const ULONG ulSize = pdxltabdesc->UlArity();

	// add mapping information for columns
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		const CDXLColDescr *pdxlcd = pdxltabdesc->Pdxlcd(ul);
		this->Insert
				(
				ulQueryLevel,
				ulRTEIndex,
				pdxlcd->IAttno(),
				pdxlcd->UlID(),
				pdxlcd->Pmdname()->Pstr()->PStrCopy(m_pmp)
				);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadIndexColumns
//
//	@doc:
//		Load up information from GPDB index and corresponding
//		optimizer table descriptor
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadIndexColumns
	(
	ULONG ulQueryLevel,
	ULONG ulRTEIndex,
	const IMDIndex *pmdindex,
	const CDXLTableDescr *pdxltabdesc
	)
{
	GPOS_ASSERT(NULL != pdxltabdesc);

	const ULONG ulSize = pmdindex->UlKeys();

	// add mapping information for columns
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		ULONG ulPos = pmdindex->UlKey(ul);
		const CDXLColDescr *pdxlcd = pdxltabdesc->Pdxlcd(ulPos);
		this->Insert
				(
				ulQueryLevel,
				ulRTEIndex,
				INT(ul + 1),
				pdxlcd->UlID(),
				pdxlcd->Pmdname()->Pstr()->PStrCopy(m_pmp)
				);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::Load
//
//	@doc:
//		Load column mapping information from list of column names
//
//---------------------------------------------------------------------------
void
CMappingVarColId::Load
	(
	ULONG ulQueryLevel,
	ULONG ulRTEIndex,
	CIdGenerator *pidgtor,
	List *plColNames
	)
{
	ListCell *plcCol = NULL;
	ULONG ul = 0;

	// add mapping information for columns
	ForEach(plcCol, plColNames)
	{
		Value *pvalue = (Value *) lfirst(plcCol);
		CHAR *szColName = strVal(pvalue);

		CWStringDynamic *pstrColName = CDXLUtils::PstrFromSz(m_pmp, szColName);

		this->Insert
				(
				ulQueryLevel,
				ulRTEIndex,
				INT(ul + 1),
				pidgtor->UlNextId(),
				pstrColName->PStrCopy(m_pmp)
				);

		ul ++;
		GPOS_DELETE(pstrColName);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadColumns
//
//	@doc:
//		Load up columns information from the array of column descriptors
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadColumns
	(
	ULONG ulQueryLevel,
	ULONG ulRTEIndex,
	const DrgPdxlcd *pdrgdxlcd
	)
{
	GPOS_ASSERT(NULL != pdrgdxlcd);
	const ULONG ulSize = pdrgdxlcd->UlLength();

	// add mapping information for columns
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		const CDXLColDescr *pdxlcd = (*pdrgdxlcd)[ul];
		this->Insert
				(
				ulQueryLevel,
				ulRTEIndex,
				pdxlcd->IAttno(),
				pdxlcd->UlID(),
				pdxlcd->Pmdname()->Pstr()->PStrCopy(m_pmp)
				);
	}

}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadDerivedTblColumns
//
//	@doc:
//		Load up information from column information in derived tables
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadDerivedTblColumns
	(
	ULONG ulQueryLevel,
	ULONG ulRTEIndex,
	const DrgPdxln *pdrgpdxlnDerivedColumns,
	List *plTargetList
	)
{
	GPOS_ASSERT(NULL != pdrgpdxlnDerivedColumns);
	GPOS_ASSERT( (ULONG) gpdb::UlListLength(plTargetList) >= pdrgpdxlnDerivedColumns->UlLength());

	ULONG ulDrvdTblColCounter = 0; // counter for the dynamic array of DXL nodes
	ListCell *plc = NULL;
	ForEach (plc, plTargetList)
	{
		TargetEntry *pte  = (TargetEntry*) lfirst(plc);
		if (!pte->resjunk)
		{
			GPOS_ASSERT(0 < pte->resno);
			CDXLNode *pdxln = (*pdrgpdxlnDerivedColumns)[ulDrvdTblColCounter];
			GPOS_ASSERT(NULL != pdxln);
			CDXLScalarIdent *pdxlnIdent = CDXLScalarIdent::PdxlopConvert(pdxln->Pdxlop());
			const CDXLColRef *pdxlcr = pdxlnIdent->Pdxlcr();
			this->Insert
					(
					ulQueryLevel,
					ulRTEIndex,
					INT(pte->resno),
					pdxlcr->UlID(),
					pdxlcr->Pmdname()->Pstr()->PStrCopy(m_pmp)
					);
			ulDrvdTblColCounter++;
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadCTEColumns
//
//	@doc:
//		Load CTE column mappings
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadCTEColumns
	(
	ULONG ulQueryLevel,
	ULONG ulRTEIndex,
	const DrgPul *pdrgpulCTEColumns,
	List *plTargetList
	)
{
	GPOS_ASSERT(NULL != pdrgpulCTEColumns);
	GPOS_ASSERT( (ULONG) gpdb::UlListLength(plTargetList) >= pdrgpulCTEColumns->UlLength());

	ULONG ulCTE = 0;
	ListCell *plc = NULL;
	ForEach (plc, plTargetList)
	{
		TargetEntry *pte  = (TargetEntry*) lfirst(plc);
		if (!pte->resjunk)
		{
			GPOS_ASSERT(0 < pte->resno);
			ULONG ulCTEColId = *((*pdrgpulCTEColumns)[ulCTE]);
			
			CWStringDynamic *pstrColName = CDXLUtils::PstrFromSz(m_pmp, pte->resname);
			this->Insert
					(
					ulQueryLevel,
					ulRTEIndex,
					INT(pte->resno),
					ulCTEColId,
					pstrColName
					);
			ulCTE++;
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::LoadProjectElements
//
//	@doc:
//		Load up information from projection list created from GPDB join expression
//
//---------------------------------------------------------------------------
void
CMappingVarColId::LoadProjectElements
	(
	ULONG ulQueryLevel,
	ULONG ulRTEIndex,
	const CDXLNode *pdxlnPrL
	)
{
	GPOS_ASSERT(NULL != pdxlnPrL);
	const ULONG ulSize = pdxlnPrL->UlArity();
	// add mapping information for columns
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		CDXLNode *pdxln = (*pdxlnPrL)[ul];
		CDXLScalarProjElem *pdxlopPrEl = CDXLScalarProjElem::PdxlopConvert(pdxln->Pdxlop());
		this->Insert
				(
				ulQueryLevel,
				ulRTEIndex,
				INT(ul + 1),
				pdxlopPrEl->UlId(),
				pdxlopPrEl->PmdnameAlias()->Pstr()->PStrCopy(m_pmp)
				);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::PmapvarcolidCopy
//
//	@doc:
//		Create a deep copy
//
//---------------------------------------------------------------------------
CMappingVarColId *
CMappingVarColId::PmapvarcolidCopy
	(
	ULONG ulQueryLevel
	)
	const
{
	CMappingVarColId *pmapvarcolid = GPOS_NEW(m_pmp) CMappingVarColId(m_pmp);

	// iterate over full map
	CMVCMapIter mvcmi(this->m_pmvcmap);
	while (mvcmi.FAdvance())
	{
		const CGPDBAttOptCol *pgpdbattoptcol = mvcmi.Pt();
		const CGPDBAttInfo *pgpdbattinfo = pgpdbattoptcol->Pgpdbattinfo();
		const COptColInfo *poptcolinfo = pgpdbattoptcol->Poptcolinfo();

		if (pgpdbattinfo->UlQueryLevel() <= ulQueryLevel)
		{
			// include all variables defined at same query level or before
			CGPDBAttInfo *pgpdbattinfoNew = GPOS_NEW(m_pmp) CGPDBAttInfo(pgpdbattinfo->UlQueryLevel(), pgpdbattinfo->UlVarNo(), pgpdbattinfo->IAttNo());
			COptColInfo *poptcolinfoNew = GPOS_NEW(m_pmp) COptColInfo(poptcolinfo->UlColId(), GPOS_NEW(m_pmp) CWStringConst(m_pmp, poptcolinfo->PstrColName()->Wsz()));
			pgpdbattinfoNew->AddRef();
			CGPDBAttOptCol *pgpdbattoptcolNew = GPOS_NEW(m_pmp) CGPDBAttOptCol(pgpdbattinfoNew, poptcolinfoNew);

			// insert into hashmap
#ifdef GPOS_DEBUG
			BOOL fResult =
#endif // GPOS_DEBUG
					pmapvarcolid->m_pmvcmap->FInsert(pgpdbattinfoNew, pgpdbattoptcolNew);
			GPOS_ASSERT(fResult);
		}
	}

	return pmapvarcolid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::PmapvarcolidCopy
//
//	@doc:
//		Create a deep copy
//
//---------------------------------------------------------------------------
CMappingVarColId *
CMappingVarColId::PmapvarcolidCopy
	(
	IMemoryPool *pmp
	)
	const
{
	CMappingVarColId *pmapvarcolid = GPOS_NEW(pmp) CMappingVarColId(pmp);

	// iterate over full map
	CMVCMapIter mvcmi(this->m_pmvcmap);
	while (mvcmi.FAdvance())
	{
		const CGPDBAttOptCol *pgpdbattoptcol = mvcmi.Pt();
		const CGPDBAttInfo *pgpdbattinfo = pgpdbattoptcol->Pgpdbattinfo();
		const COptColInfo *poptcolinfo = pgpdbattoptcol->Poptcolinfo();

		CGPDBAttInfo *pgpdbattinfoNew = GPOS_NEW(pmp) CGPDBAttInfo(pgpdbattinfo->UlQueryLevel(), pgpdbattinfo->UlVarNo(), pgpdbattinfo->IAttNo());
		COptColInfo *poptcolinfoNew = GPOS_NEW(pmp) COptColInfo(poptcolinfo->UlColId(), GPOS_NEW(pmp) CWStringConst(pmp, poptcolinfo->PstrColName()->Wsz()));
		pgpdbattinfoNew->AddRef();
		CGPDBAttOptCol *pgpdbattoptcolNew = GPOS_NEW(pmp) CGPDBAttOptCol(pgpdbattinfoNew, poptcolinfoNew);

		// insert into hashmap
#ifdef GPOS_DEBUG
	BOOL fResult =
#endif // GPOS_DEBUG
		pmapvarcolid->m_pmvcmap->FInsert(pgpdbattinfoNew, pgpdbattoptcolNew);
		GPOS_ASSERT(fResult);
	}

	return pmapvarcolid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingVarColId::PmapvarcolidRemap
//
//	@doc:
//		Create a copy of the mapping replacing the old column ids by new ones
//
//---------------------------------------------------------------------------
CMappingVarColId *
CMappingVarColId::PmapvarcolidRemap
	(
	IMemoryPool *pmp,
	DrgPul *pdrgpulOld,
	DrgPul *pdrgpulNew
	)
	const
{
	GPOS_ASSERT(NULL != pdrgpulOld);
	GPOS_ASSERT(NULL != pdrgpulNew);
	GPOS_ASSERT(pdrgpulNew->UlLength() == pdrgpulOld->UlLength());
	
	// construct a mapping old cols -> new cols
	HMUlUl *phmulul = CTranslatorUtils::PhmululMap(pmp, pdrgpulOld, pdrgpulNew);
		
	CMappingVarColId *pmapvarcolid = GPOS_NEW(pmp) CMappingVarColId(pmp);

	CMVCMapIter mvcmi(this->m_pmvcmap);
	while (mvcmi.FAdvance())
	{
		const CGPDBAttOptCol *pgpdbattoptcol = mvcmi.Pt();
		const CGPDBAttInfo *pgpdbattinfo = pgpdbattoptcol->Pgpdbattinfo();
		const COptColInfo *poptcolinfo = pgpdbattoptcol->Poptcolinfo();

		CGPDBAttInfo *pgpdbattinfoNew = GPOS_NEW(pmp) CGPDBAttInfo(pgpdbattinfo->UlQueryLevel(), pgpdbattinfo->UlVarNo(), pgpdbattinfo->IAttNo());
		ULONG ulColId = poptcolinfo->UlColId();
		ULONG *pulColIdNew = phmulul->PtLookup(&ulColId);
		if (NULL != pulColIdNew)
		{
			ulColId = *pulColIdNew;
		}
		
		COptColInfo *poptcolinfoNew = GPOS_NEW(pmp) COptColInfo(ulColId, GPOS_NEW(pmp) CWStringConst(pmp, poptcolinfo->PstrColName()->Wsz()));
		pgpdbattinfoNew->AddRef();
		CGPDBAttOptCol *pgpdbattoptcolNew = GPOS_NEW(pmp) CGPDBAttOptCol(pgpdbattinfoNew, poptcolinfoNew);

#ifdef GPOS_DEBUG
		BOOL fResult =
#endif // GPOS_DEBUG
		pmapvarcolid->m_pmvcmap->FInsert(pgpdbattinfoNew, pgpdbattoptcolNew);
		GPOS_ASSERT(fResult);
	}
	
	phmulul->Release();

	return pmapvarcolid;
}

// EOF
