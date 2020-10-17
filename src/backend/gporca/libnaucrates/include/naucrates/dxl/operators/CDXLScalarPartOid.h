//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CDXLScalarPartOid.h
//
//	@doc:
//		Class for representing DXL Part Oid expressions
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarPartOid_H
#define GPDXL_CDXLScalarPartOid_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarPartOid
//
//	@doc:
//		Class for representing DXL Part Oid expressions
//		These oids are created and consumed by the PartitionSelector operator
//
//---------------------------------------------------------------------------
class CDXLScalarPartOid : public CDXLScalar
{
private:
	// partitioning level
	ULONG m_partitioning_level;

	// private copy ctor
	CDXLScalarPartOid(const CDXLScalarPartOid &);

public:
	// ctor
	CDXLScalarPartOid(CMemoryPool *mp, ULONG partitioning_level);

	// operator type
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// partitioning level
	ULONG
	GetPartitioningLevel() const
	{
		return m_partitioning_level;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	virtual void AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarPartOid *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarPartOid == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarPartOid *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarPartOid_H

// EOF
