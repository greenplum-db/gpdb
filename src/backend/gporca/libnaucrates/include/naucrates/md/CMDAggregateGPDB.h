//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDAggregateGPDB.h
//
//	@doc:
//		Class for representing for GPDB-specific aggregates in the metadata cache
//---------------------------------------------------------------------------



#ifndef GPMD_CMDAggregateGPDB_H
#define GPMD_CMDAggregateGPDB_H

#include "gpos/base.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDAggregate.h"


namespace gpmd
{
using namespace gpos;


//---------------------------------------------------------------------------
//	@class:
//		CMDAggregateGPDB
//
//	@doc:
//		Class for representing GPDB-specific aggregates in the metadata
//		cache
//
//---------------------------------------------------------------------------
class CMDAggregateGPDB : public IMDAggregate
{
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// aggregate id
	IMDId *m_mdid;

	// aggregate name
	CMDName *m_mdname;

	// result type
	IMDId *m_mdid_type_result;

	// type of intermediate results
	IMDId *m_mdid_type_intermediate;

	// is aggregate ordered
	BOOL m_is_ordered;

	// is aggregate splittable
	BOOL m_is_splittable;

	// is aggregate hash capable
	BOOL m_hash_agg_capable;

	// is aggregate replication slice safe for execution
	BOOL m_is_repsafe;

public:
	CMDAggregateGPDB(const CMDAggregateGPDB &) = delete;

	// ctor
	CMDAggregateGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
					 IMDId *result_type_mdid,
					 IMDId *intermediate_result_type_mdid, BOOL is_ordered_agg,
					 BOOL is_splittable, BOOL is_hash_agg_capable,
					 bool is_repsafe);

	//dtor
	~CMDAggregateGPDB() override;

	// string representation of object
	const CWStringDynamic *
	GetStrRepr() const override
	{
		return m_dxl_str;
	}

	// aggregate id
	IMDId *MDId() const override;

	// aggregate name
	CMDName Mdname() const override;

	// result id
	IMDId *GetResultTypeMdid() const override;

	// intermediate result id
	IMDId *GetIntermediateResultTypeMdid() const override;

	// serialize object in DXL format
	void Serialize(gpdxl::CXMLSerializer *xml_serializer) const override;

	// is an ordered aggregate
	BOOL
	IsOrdered() const override
	{
		return m_is_ordered;
	}

	// is aggregate splittable
	BOOL
	IsSplittable() const override
	{
		return m_is_splittable;
	}

	// is aggregate hash capable
	BOOL
	IsHashAggCapable() const override
	{
		return m_hash_agg_capable;
	}

	// is aggregate replicate slice execution safe
	BOOL
	IsAggRepSafe() const override
	{
		return m_is_repsafe;
	}

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDAggregateGPDB_H

// EOF
