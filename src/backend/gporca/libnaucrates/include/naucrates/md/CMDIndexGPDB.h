//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDIndexGPDB.h
//
//	@doc:
//		Implementation of indexes in the metadata cache
//---------------------------------------------------------------------------



#ifndef GPMD_CMDIndexGPDB_H
#define GPMD_CMDIndexGPDB_H

#include "gpos/base.h"

#include "naucrates/md/IMDIndex.h"

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

// fwd decl
class IMDPartConstraint;

//---------------------------------------------------------------------------
//	@class:
//		CMDIndexGPDB
//
//	@doc:
//		Class for indexes in the metadata cache
//
//---------------------------------------------------------------------------
class CMDIndexGPDB : public IMDIndex
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// index mdid
	IMDId *m_mdid;

	// table name
	CMDName *m_mdname;

	// is the index clustered
	BOOL m_clustered;

	// is the index partitioned
	BOOL m_partitioned;

	// index type
	EmdindexType m_index_type;

	// type of items returned by index
	IMDId *m_mdid_item_type;

	// index key columns
	ULongPtrArray *m_index_key_cols_array;

	// included columns
	ULongPtrArray *m_included_cols_array;

	// operator families for each index key
	IMdIdArray *m_mdid_opfamilies_array;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// Child index oids
	IMdIdArray *m_child_index_oids;

public:
	CMDIndexGPDB(const CMDIndexGPDB &) = delete;

	// ctor
	CMDIndexGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
				 BOOL is_clustered, BOOL is_partitioned,
				 EmdindexType index_type, IMDId *mdid_item_type,
				 ULongPtrArray *index_key_cols_array,
				 ULongPtrArray *included_cols_array,
				 IMdIdArray *mdid_opfamilies_array,
				 IMdIdArray *child_index_oids);

	// dtor
	~CMDIndexGPDB() override;

	// index mdid
	IMDId *MDId() const override;

	// index name
	CMDName Mdname() const override;

	// is the index clustered
	BOOL IsClustered() const override;

	// is the index partitioned
	BOOL IsPartitioned() const override;

	// index type
	EmdindexType IndexType() const override;

	// number of keys
	ULONG Keys() const override;

	// return the n-th key column
	ULONG KeyAt(ULONG pos) const override;

	// return the position of the key column
	ULONG GetKeyPos(ULONG column) const override;

	// number of included columns
	ULONG IncludedCols() const override;

	// return the n-th included column
	ULONG IncludedColAt(ULONG pos) const override;

	// return the position of the included column
	ULONG GetIncludedColPos(ULONG column) const override;

	// DXL string for index
	const CWStringDynamic *
	GetStrRepr() const override
	{
		return m_dxl_str;
	}

	// serialize MD index in DXL format given a serializer object
	void Serialize(gpdxl::CXMLSerializer *) const override;

	// type id of items returned by the index
	IMDId *GetIndexRetItemTypeMdid() const override;

	// check if given scalar comparison can be used with the index key
	// at the specified position
	BOOL IsCompatible(const IMDScalarOp *md_scalar_op,
					  ULONG key_pos) const override;

	// child index oids
	IMdIdArray *ChildIndexMdids() const override;

#ifdef GPOS_DEBUG
	// debug print of the MD index
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDIndexGPDB_H

// EOF
