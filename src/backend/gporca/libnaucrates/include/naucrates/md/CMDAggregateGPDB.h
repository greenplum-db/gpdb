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

	// private copy ctor
	CMDAggregateGPDB(const CMDAggregateGPDB &);

public:
	// ctor
	CMDAggregateGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
					 IMDId *result_type_mdid,
					 IMDId *intermediate_result_type_mdid, BOOL is_ordered_agg,
					 BOOL is_splittable, BOOL is_hash_agg_capable);

	//dtor
	~CMDAggregateGPDB();

	// string representation of object
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	// aggregate id
	virtual IMDId *MDId() const;

	// aggregate name
	virtual CMDName Mdname() const;

	// result id
	virtual IMDId *GetResultTypeMdid() const;

	// intermediate result id
	virtual IMDId *GetIntermediateResultTypeMdid() const;

	// serialize object in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;

	// is an ordered aggregate
	virtual BOOL
	IsOrdered() const
	{
		return m_is_ordered;
	}

	// is aggregate splittable
	virtual BOOL
	IsSplittable() const
	{
		return m_is_splittable;
	}

	// is aggregate hash capable
	virtual BOOL
	IsHashAggCapable() const
	{
		return m_hash_agg_capable;
	}

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	virtual void DebugPrint(IOstream &os) const;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDAggregateGPDB_H

// EOF
