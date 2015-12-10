//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLTranslateContextBaseTable.cpp
//
//	@doc:
//		Implementation of the methods for accessing translation context for base tables.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"
#include "gpopt/translate/CDXLTranslateContextBaseTable.h"

using namespace gpdxl;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::CDXLTranslateContextBaseTable
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLTranslateContextBaseTable::CDXLTranslateContextBaseTable
	(
	IMemoryPool *pmp
	)
	:
	m_pmp(pmp),
	m_oid(InvalidOid),
	m_iRel(0)
{
	// initialize hash table
	m_phmuli = GPOS_NEW(m_pmp) HMUlI(m_pmp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::~CDXLTranslateContextBaseTable
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLTranslateContextBaseTable::~CDXLTranslateContextBaseTable()
{
	CRefCount::SafeRelease(m_phmuli);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::SetOID
//
//	@doc:
//		Set the oid of the base relation
//
//---------------------------------------------------------------------------
void
CDXLTranslateContextBaseTable::SetOID
	(
	OID oid
	)
{
	GPOS_ASSERT(oid != InvalidOid);
	m_oid = oid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::SetIdx
//
//	@doc:
//		Set the index of the base relation in the range table
//
//---------------------------------------------------------------------------
void
CDXLTranslateContextBaseTable::SetIdx
	(
	Index iRel
	)
{
	GPOS_ASSERT(0 < iRel);
	m_iRel = iRel;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::OidRel
//
//	@doc:
//		Returns the oid of the table
//
//---------------------------------------------------------------------------
OID
CDXLTranslateContextBaseTable::OidRel() const
{
	return m_oid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::IRel
//
//	@doc:
//		Returns the index of the relation in the rable table
//
//---------------------------------------------------------------------------
Index
CDXLTranslateContextBaseTable::IRel() const
{
	GPOS_ASSERT(0 < m_iRel);
	return m_iRel;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::IAttnoForColId
//
//	@doc:
//		Lookup the index of the attribute with the DXL col id in the underlying table schema
//
//---------------------------------------------------------------------------
INT
CDXLTranslateContextBaseTable::IAttnoForColId
	(
	ULONG ulColId
	)
	const
{
	const INT *pi = m_phmuli->PtLookup(&ulColId);
	if (NULL != pi)
	{
		return *pi;
	}

	// column not found
	return 0;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::FInsertMapping
//
//	@doc:
//		Insert a mapping ColId->Idx, where ulDXLColId is a DXL introduced column id,
//		and ulIdx is the index of the column in the underlying table schema
//
//---------------------------------------------------------------------------
BOOL
CDXLTranslateContextBaseTable::FInsertMapping
	(
	ULONG ulDXLColId,
	INT iAttno
	)
{
	// copy key and value
	ULONG *pulKey = GPOS_NEW(m_pmp) ULONG(ulDXLColId);
	INT *piValue = GPOS_NEW(m_pmp) INT(iAttno);

	// insert colid-idx mapping in the hash map

	BOOL fRes = m_phmuli->FInsert(pulKey, piValue);

	GPOS_ASSERT(fRes);

	return fRes;
}

// EOF
