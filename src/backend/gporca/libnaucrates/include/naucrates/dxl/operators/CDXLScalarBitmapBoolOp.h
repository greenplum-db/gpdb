//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarBitmapBoolOp.h
//
//	@doc:
//		Class for representing DXL bitmap boolean operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarBitmapBoolOp_H
#define GPDXL_CDXLScalarBitmapBoolOp_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarBitmapBoolOp
//
//	@doc:
//		Class for representing DXL bitmap boolean operator
//
//---------------------------------------------------------------------------
class CDXLScalarBitmapBoolOp : public CDXLScalar
{
public:
	// type of bitmap operator
	enum EdxlBitmapBoolOp
	{
		EdxlbitmapAnd,
		EdxlbitmapOr,
		EdxlbitmapSentinel
	};

private:
	// type id
	IMDId *m_mdid_type;

	// operator type
	const EdxlBitmapBoolOp m_bitmap_op_type;

public:
	CDXLScalarBitmapBoolOp(const CDXLScalarBitmapBoolOp &) = delete;

	// ctor
	CDXLScalarBitmapBoolOp(CMemoryPool *mp, IMDId *mdid_type,
						   EdxlBitmapBoolOp bitmap_op_type);

	// dtor
	~CDXLScalarBitmapBoolOp() override;

	// dxl operator type
	Edxlopid GetDXLOperator() const override;

	// bitmap operator type
	EdxlBitmapBoolOp GetDXLBitmapOpType() const;

	// return type
	IMDId *MdidType() const;

	// name of the DXL operator name
	const CWStringConst *GetOpNameStr() const override;

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;


#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarBitmapBoolOp *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarBitmapBoolOp == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarBitmapBoolOp *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarBitmapBoolOp_H

// EOF
