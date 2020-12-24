//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalValuesScan.h
//
//	@doc:
//		Class for representing DXL physical Values scan
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalValuesScan_H
#define GPDXL_CDXLPhysicalValuesScan_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
enum EdxlnVal
{
	EdxlValIndexProjList = 0,
	EdxlValIndexConstStart,
	EdxlValIndexSentinel
};

// class for representing DXL physical Values scan
class CDXLPhysicalValuesScan : public CDXLPhysical
{
private:
public:
	CDXLPhysicalValuesScan(CDXLPhysicalValuesScan &) = delete;

	// ctor
	CDXLPhysicalValuesScan(CMemoryPool *mp);

	// dtor
	~CDXLPhysicalValuesScan() override;

	// get operator type
	Edxlopid GetDXLOperator() const override;

	// get operator name
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalValuesScan *Cast(CDXLOperator *dxl_op);

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalValuesScan_H

// EOF
