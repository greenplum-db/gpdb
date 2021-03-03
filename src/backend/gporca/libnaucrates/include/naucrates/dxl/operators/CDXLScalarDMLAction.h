//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarDMLAction.h
//
//	@doc:
//		Class for representing DXL DML action expressions
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarDMLAction_H
#define GPDXL_CDXLScalarDMLAction_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarDMLAction
//
//	@doc:
//		Class for representing DXL DML action expressions
//
//---------------------------------------------------------------------------
class CDXLScalarDMLAction : public CDXLScalar
{
private:
public:
	CDXLScalarDMLAction(const CDXLScalarDMLAction &) = delete;

	// ctor/dtor
	explicit CDXLScalarDMLAction(CMemoryPool *mp);

	~CDXLScalarDMLAction() override = default;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLScalarDMLAction *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarDMLAction == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarDMLAction *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarDMLAction_H

// EOF
