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

extern "C" {
#include "postgres.h"
}
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
CDXLTranslateContextBaseTable::CDXLTranslateContextBaseTable(CMemoryPool *mp)
	: m_mp(mp), m_oid(InvalidOid), m_rel_index(0)
{
	// initialize hash table
	m_colid_to_attno_map = GPOS_NEW(m_mp) UlongToIntMap(m_mp);
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
	CRefCount::SafeRelease(m_colid_to_attno_map);
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
CDXLTranslateContextBaseTable::SetOID(OID oid)
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
CDXLTranslateContextBaseTable::SetRelIndex(Index rel_index)
{
	GPOS_ASSERT(0 < rel_index);
	m_rel_index = rel_index;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::GetOid
//
//	@doc:
//		Returns the oid of the table
//
//---------------------------------------------------------------------------
OID
CDXLTranslateContextBaseTable::GetOid() const
{
	return m_oid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::GetRelIndex
//
//	@doc:
//		Returns the index of the relation in the rable table
//
//---------------------------------------------------------------------------
Index
CDXLTranslateContextBaseTable::GetRelIndex() const
{
	GPOS_ASSERT(0 < m_rel_index);
	return m_rel_index;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::GetAttnoForColId
//
//	@doc:
//		Lookup the index of the attribute with the DXL col id in the underlying table schema
//
//---------------------------------------------------------------------------
INT
CDXLTranslateContextBaseTable::GetAttnoForColId(ULONG colid) const
{
	const INT *pi = m_colid_to_attno_map->Find(&colid);
	if (nullptr != pi)
	{
		return *pi;
	}

	// column not found
	return 0;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTranslateContextBaseTable::InsertMapping
//
//	@doc:
//		Insert a mapping ColId->Idx, where ulDXLColId is a DXL introduced column id,
//		and ulIdx is the index of the column in the underlying table schema
//
//---------------------------------------------------------------------------
BOOL
CDXLTranslateContextBaseTable::InsertMapping(ULONG dxl_colid, INT att_no)
{
	// copy key and value
	ULONG *key = GPOS_NEW(m_mp) ULONG(dxl_colid);
	INT *value = GPOS_NEW(m_mp) INT(att_no);

	// insert colid-idx mapping in the hash map

	BOOL res = m_colid_to_attno_map->Insert(key, value);

	GPOS_ASSERT(res);

	return res;
}

// EOF
