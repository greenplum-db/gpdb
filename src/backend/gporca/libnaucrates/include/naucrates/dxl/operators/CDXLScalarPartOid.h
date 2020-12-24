//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
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

public:
	CDXLScalarPartOid(const CDXLScalarPartOid &) = delete;

	// ctor
	CDXLScalarPartOid(CMemoryPool *mp, ULONG partitioning_level);

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// partitioning level
	ULONG
	GetPartitioningLevel() const
	{
		return m_partitioning_level;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
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
