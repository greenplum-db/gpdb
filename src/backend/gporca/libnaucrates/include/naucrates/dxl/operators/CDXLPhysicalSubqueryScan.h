//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalSubqueryScan.h
//
//	@doc:
//		Class for representing DXL physical subquery scan (projection) operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalSubqueryScan_H
#define GPDXL_CDXLPhysicalSubqueryScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/md/CMDName.h"


namespace gpdxl
{
using namespace gpmd;

// indices of subquery scan elements in the children array
enum Edxlsubqscan
{
	EdxlsubqscanIndexProjList = 0,
	EdxlsubqscanIndexFilter,
	EdxlsubqscanIndexChild,
	EdxlsubqscanIndexSentinel
};
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalSubqueryScan
//
//	@doc:
//		Class for representing DXL physical subquery scan (projection) operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalSubqueryScan : public CDXLPhysical
{
private:
	// name for the subquery scan node (corresponding to name in GPDB's SubqueryScan)
	CMDName *m_mdname_alias;

public:
	CDXLPhysicalSubqueryScan(CDXLPhysicalSubqueryScan &) = delete;

	// ctor/dtor
	CDXLPhysicalSubqueryScan(CMemoryPool *mp, CMDName *mdname);

	~CDXLPhysicalSubqueryScan() override;

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;
	const CMDName *MdName();

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalSubqueryScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalSubqueryScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalSubqueryScan *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalSubqueryScan_H

// EOF
