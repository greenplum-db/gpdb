//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 Broadcom
//
//	@filename:
//		CDXLScalarParam.h
//
//	@doc:
//		Class for representing DXL scalar parameters.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarParam_H
#define GPDXL_CDXLScalarParam_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarParam
//
//	@doc:
//		Class for representing DXL scalar parameters
//
//---------------------------------------------------------------------------
class CDXLScalarParam : public CDXLScalar
{
private:
	// param id
	ULONG m_id;

	// param type
	IMDId *m_mdid_type;

	// param type modifier
	INT m_type_modifer;

public:
	CDXLScalarParam(CDXLScalarParam &) = delete;

	// ctor/dtor
	CDXLScalarParam(CMemoryPool *, ULONG, IMDId *, INT);

	~CDXLScalarParam() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// accessors
	ULONG GetId() const;

	IMDId *GetMDIdType() const;

	INT GetTypeModifier() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLScalarParam *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarParam == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarParam *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure
	void AssertValid(const CDXLNode *node,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl



#endif	// !GPDXL_CDXLScalarParam_H
