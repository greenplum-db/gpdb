//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CStateDXLToQuery.cpp
//
//	@doc:
//		Implementation of the functions that provide access to the list of
//		output target entries and output column names during DXL-->Query
//		translation.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"

#include "nodes/makefuncs.h"

#include "gpopt/translate/CStateDXLToQuery.h"
#include "gpos/base.h"
#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::CStateDXLToQuery
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CStateDXLToQuery::CStateDXLToQuery
	(
	IMemoryPool *pmp
	)
	:
	m_pmp(pmp),
	m_plTEColumns(NIL),
	m_plColumnNames(NIL)
{
	m_pdrgpulColIds = GPOS_NEW(m_pmp) DrgPul(m_pmp);
}


//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::~CStateDXLToQuery
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CStateDXLToQuery::~CStateDXLToQuery()
{
	gpdb::FreeListAndNull(&m_plColumnNames);
	m_pdrgpulColIds->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::AddOutputColumnEntry
//
//	@doc:
//		Adding a new column target entry along with its column name
//
//---------------------------------------------------------------------------
void
CStateDXLToQuery::AddOutputColumnEntry
	(
	TargetEntry *pte,
	CHAR *szColumnName,
	ULONG ulColId
	)
{
	GPOS_ASSERT(gpdb::UlListLength(m_plTEColumns) == gpdb::UlListLength(m_plColumnNames));
	GPOS_ASSERT((ULONG) gpdb::UlListLength(m_plTEColumns) == m_pdrgpulColIds->UlLength());
	m_plTEColumns = gpdb::PlAppendElement(m_plTEColumns, pte);
	m_plColumnNames = gpdb::PlAppendElement(m_plColumnNames, szColumnName);
	m_pdrgpulColIds->Append(GPOS_NEW(m_pmp) ULONG(ulColId));
}


//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::FTEFound
//
//	@doc:
//		Check to see if the target entry is already in the state's list of
//		target entries
//
//---------------------------------------------------------------------------
BOOL
CStateDXLToQuery::FTEFound(TargetEntry *pte)
{
	// check if we have already added the corresponding pte entry in pplTE
	TargetEntry *pteFound = gpdb::PteMember( (Node*) pte->expr, m_plTEColumns);

	if(NULL == pteFound)
	{
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::Clear
//
//	@doc:
//		Clear the lists
//
//---------------------------------------------------------------------------
void
CStateDXLToQuery::Clear()
{
	gpdb::FreeListAndNull(&m_plColumnNames);
	m_plColumnNames = NIL;
	m_plTEColumns = NIL;
	m_pdrgpulColIds->Clear();
}

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::Reload
//
//	@doc:
//
// 		Clear entries and load content from another state
//---------------------------------------------------------------------------
void
CStateDXLToQuery::Reload
	(
	CStateDXLToQuery *pstatedxltoquery
	)
{
	Clear();

	const ULONG ulSize = pstatedxltoquery->UlLength();
	for (ULONG ul = 0; ul < ulSize ; ul++)
	{
		TargetEntry *pte = pstatedxltoquery->PteColumn(ul);
		GPOS_ASSERT(NULL != pte);
		ULONG ulColId = pstatedxltoquery->UlColId(ul);
		TargetEntry *pteCopy =  (TargetEntry*) gpdb::PvCopyObject(pte);
		AddOutputColumnEntry(pteCopy, pteCopy->resname, ulColId);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::UlLength
//
//	@doc:
//		Return size of the list of target entries
//
//---------------------------------------------------------------------------
ULONG
CStateDXLToQuery::UlLength()
{
	const ULONG ulSize = (ULONG) gpdb::UlListLength(m_plTEColumns);
	GPOS_ASSERT(ulSize == (ULONG) gpdb::UlListLength(m_plColumnNames));
	return ulSize;
}

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::PteColumn
//
//	@doc:
//		Return the element from the list of target entries at the specified
//		index
//
//---------------------------------------------------------------------------
TargetEntry *
CStateDXLToQuery::PteColumn(ULONG ulIndex)
{
	GPOS_ASSERT((ULONG) gpdb::UlListLength(m_plTEColumns) > ulIndex);
	return (TargetEntry*) gpdb::PvListNth(m_plTEColumns, ulIndex);
}

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::SzColumnName
//
//	@doc:
//		Return the ulIndex^{th} element from the list of column names
//
//---------------------------------------------------------------------------
CHAR *
CStateDXLToQuery::SzColumnName(ULONG ulIndex)
{
	GPOS_ASSERT((ULONG) gpdb::UlListLength(m_plColumnNames) > ulIndex);
	return (CHAR*) gpdb::PvListNth(m_plColumnNames, ulIndex);
}

//---------------------------------------------------------------------------
//	@function:
//		CStateDXLToQuery::UlColId
//
//	@doc:
//		Return the ulIndex^{th} element from the list of column names
//
//---------------------------------------------------------------------------
ULONG
CStateDXLToQuery::UlColId(ULONG ulIndex)
{
	GPOS_ASSERT(m_pdrgpulColIds->UlLength() > ulIndex);

	ULONG *pulColId = (*m_pdrgpulColIds)[ulIndex];
	GPOS_ASSERT(NULL != pulColId);

	return *(pulColId);
}

// EOF
