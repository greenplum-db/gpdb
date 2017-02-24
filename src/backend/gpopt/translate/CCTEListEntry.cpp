//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CCTEListEntry.cpp
//
//	@doc:
//		Implementation of the class representing the list of common table
//		expression defined at a query level
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"
#include "gpopt/translate/CCTEListEntry.h"

#include "nodes/parsenodes.h"

#include "gpos/base.h"
#include "gpopt/gpdbwrappers.h"
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CCTEListEntry::CCTEListEntry
//
//	@doc:
//		Ctor: single CTE
//
//---------------------------------------------------------------------------
CCTEListEntry::CCTEListEntry
	(
	IMemoryPool *pmp,
	ULONG ulQueryLevel,
	CommonTableExpr *pcte,
	CDXLNode *pdxlnCTEProducer
	)
	:
	m_ulQueryLevel(ulQueryLevel),
	m_phmszcteinfo(NULL)
{
	GPOS_ASSERT(NULL != pcte && NULL != pdxlnCTEProducer);
	
	m_phmszcteinfo = GPOS_NEW(pmp) HMSzCTEInfo(pmp);
	Query *pqueryCTE = (Query*) pcte->ctequery;
		
#ifdef GPOS_DEBUG
		BOOL fResult =
#endif
	m_phmszcteinfo->FInsert(pcte->ctename, GPOS_NEW(pmp) SCTEProducerInfo(pdxlnCTEProducer, pqueryCTE->targetList));
		
	GPOS_ASSERT(fResult);
}

//---------------------------------------------------------------------------
//	@function:
//		CCTEListEntry::CCTEListEntry
//
//	@doc:
//		Ctor: multiple CTEs
//
//---------------------------------------------------------------------------
CCTEListEntry::CCTEListEntry
	(
	IMemoryPool *pmp,
	ULONG ulQueryLevel,
	List *plCTE, 
	DrgPdxln *pdrgpdxln
	)
	:
	m_ulQueryLevel(ulQueryLevel),
	m_phmszcteinfo(NULL)
{
	GPOS_ASSERT(NULL != pdrgpdxln);
	GPOS_ASSERT(pdrgpdxln->UlLength() == gpdb::UlListLength(plCTE));
	
	m_phmszcteinfo = GPOS_NEW(pmp) HMSzCTEInfo(pmp);
	const ULONG ulCTEs = pdrgpdxln->UlLength();
	
	for (ULONG ul = 0; ul < ulCTEs; ul++)
	{
		CDXLNode *pdxlnCTEProducer = (*pdrgpdxln)[ul];
		CommonTableExpr *pcte = (CommonTableExpr*) gpdb::PvListNth(plCTE, ul);

		Query *pqueryCTE = (Query*) pcte->ctequery;
		
#ifdef GPOS_DEBUG
		BOOL fResult =
#endif
		m_phmszcteinfo->FInsert(pcte->ctename, GPOS_NEW(pmp) SCTEProducerInfo(pdxlnCTEProducer, pqueryCTE->targetList));
		
		GPOS_ASSERT(fResult);
		GPOS_ASSERT(NULL != m_phmszcteinfo->PtLookup(pcte->ctename));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CCTEListEntry::PdxlnCTEProducer
//
//	@doc:
//		Return the query of the CTE referenced in the range table entry
//
//---------------------------------------------------------------------------
const CDXLNode *
CCTEListEntry::PdxlnCTEProducer
	(
	const CHAR *szCTE
	)
	const
{
	SCTEProducerInfo *pcteinfo = m_phmszcteinfo->PtLookup(szCTE);
	if (NULL == pcteinfo)
	{
		return NULL; 
	}
	
	return pcteinfo->m_pdxlnCTEProducer;
}

//---------------------------------------------------------------------------
//	@function:
//		CCTEListEntry::PdxlnCTEProducer
//
//	@doc:
//		Return the query of the CTE referenced in the range table entry
//
//---------------------------------------------------------------------------
List *
CCTEListEntry::PlCTEProducerTL
	(
	const CHAR *szCTE
	)
	const
{
	SCTEProducerInfo *pcteinfo = m_phmszcteinfo->PtLookup(szCTE);
	if (NULL == pcteinfo)
	{
		return NULL; 
	}
	
	return pcteinfo->m_plTargetList;
}

//---------------------------------------------------------------------------
//	@function:
//		CCTEListEntry::AddCTEProducer
//
//	@doc:
//		Add a new CTE producer to this query level
//
//---------------------------------------------------------------------------
void
CCTEListEntry::AddCTEProducer
	(
	IMemoryPool *pmp,
	CommonTableExpr *pcte,
	const CDXLNode *pdxlnCTEProducer
	)
{
	GPOS_ASSERT(NULL == m_phmszcteinfo->PtLookup(pcte->ctename) && "CTE entry already exists");
	Query *pqueryCTE = (Query*) pcte->ctequery;
	
#ifdef GPOS_DEBUG
	BOOL fResult =
#endif
	m_phmszcteinfo->FInsert(pcte->ctename, GPOS_NEW(pmp) SCTEProducerInfo(pdxlnCTEProducer, pqueryCTE->targetList));
	
	GPOS_ASSERT(fResult);
}

// EOF
