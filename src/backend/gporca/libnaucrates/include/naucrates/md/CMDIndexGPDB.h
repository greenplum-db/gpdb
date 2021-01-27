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

	// partition constraint
	IMDPartConstraint *m_mdpart_constraint;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// private copy ctor
	CMDIndexGPDB(const CMDIndexGPDB &);

public:
	// ctor
	CMDIndexGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
				 BOOL is_clustered, EmdindexType index_type,
				 IMDId *mdid_item_type, ULongPtrArray *index_key_cols_array,
				 ULongPtrArray *included_cols_array,
				 IMdIdArray *mdid_opfamilies_array,
				 IMDPartConstraint *mdpart_constraint);

	// dtor
	virtual ~CMDIndexGPDB();

	// index mdid
	virtual IMDId *MDId() const;

	// index name
	virtual CMDName Mdname() const;

	// is the index clustered
	virtual BOOL IsClustered() const;

	// index type
	virtual EmdindexType IndexType() const;

	// number of keys
	virtual ULONG Keys() const;

	// return the n-th key column
	virtual ULONG KeyAt(ULONG pos) const;

	// return the position of the key column
	virtual ULONG GetKeyPos(ULONG column) const;

	// number of included columns
	virtual ULONG IncludedCols() const;

	// return the n-th included column
	virtual ULONG IncludedColAt(ULONG pos) const;

	// return the position of the included column
	virtual ULONG GetIncludedColPos(ULONG column) const;

	// part constraint
	virtual IMDPartConstraint *MDPartConstraint() const;

	// DXL string for index
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	// serialize MD index in DXL format given a serializer object
	virtual void Serialize(gpdxl::CXMLSerializer *) const;

	// type id of items returned by the index
	virtual IMDId *GetIndexRetItemTypeMdid() const;

	// check if given scalar comparison can be used with the index key
	// at the specified position
	virtual BOOL IsCompatible(const IMDScalarOp *md_scalar_op,
							  ULONG key_pos) const;

#ifdef GPOS_DEBUG
	// debug print of the MD index
	virtual void DebugPrint(IOstream &os) const;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDIndexGPDB_H

// EOF
