//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLTranslateContextBaseTable.h
//
//	@doc:
//		Class providing access to translation context for translating base table
//		column references, such as mappings from a DXL ColId to a GPDB attribute number in
//		the base table schema
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLTranslateContextBaseTable_H
#define GPDXL_CDXLTranslateContextBaseTable_H


#include "gpos/base.h"
#include "gpos/common/CHashMap.h"

extern "C" {
#include "postgres.h"  // Index
}

#include "naucrates/dxl/gpdb_types.h"

namespace gpdxl
{
using namespace gpos;


//---------------------------------------------------------------------------
//	@class:
//		CDXLTranslateContextBaseTable
//
//	@doc:
//		Class providing access to translation context, such as mappings between
//		ColIds and target entries
//
//---------------------------------------------------------------------------
class CDXLTranslateContextBaseTable
{
	// hash maps mapping ULONG -> INT
	using UlongToIntMap =
		CHashMap<ULONG, INT, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
				 CleanupDelete<ULONG>, CleanupDelete<INT>>;

private:
	CMemoryPool *m_mp;

	// oid of the base table
	OID m_oid;

	// index of the relation in the rtable
	Index m_rel_index;

	// maps a colid of a column to the attribute number of that column in the schema of the underlying relation
	UlongToIntMap *m_colid_to_attno_map;

public:
	CDXLTranslateContextBaseTable(const CDXLTranslateContextBaseTable &) =
		delete;

	// ctor/dtor
	explicit CDXLTranslateContextBaseTable(CMemoryPool *mp);


	~CDXLTranslateContextBaseTable();

	// accessors
	OID GetOid() const;

	Index GetRelIndex() const;

	// return the index of the column in the base relation for the given DXL ColId
	INT GetAttnoForColId(ULONG dxl_colid) const;

	// setters
	void SetOID(OID oid);

	void SetRelIndex(Index rel_index);

	// store the mapping of the given DXL column id and index in the base relation schema
	BOOL InsertMapping(ULONG dxl_colid, INT att_no);
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLTranslateContextBaseTable_H

// EOF
