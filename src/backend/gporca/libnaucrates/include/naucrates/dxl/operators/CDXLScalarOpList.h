//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarOpList.h
//
//	@doc:
//		Class for representing DXL list of scalar expressions
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarOpList_H
#define GPDXL_CDXLScalarOpList_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarOpList
//
//	@doc:
//		Class for representing DXL list of scalar expressions
//
//---------------------------------------------------------------------------
class CDXLScalarOpList : public CDXLScalar
{
public:
	// type of the operator list
	enum EdxlOpListType
	{
		EdxloplistEqFilterList,
		EdxloplistFilterList,
		EdxloplistGeneral,
		EdxloplistSentinel
	};

private:
	// operator list type
	EdxlOpListType m_dxl_op_list_type;

public:
	CDXLScalarOpList(const CDXLScalarOpList &) = delete;

	// ctor
	CDXLScalarOpList(CMemoryPool *mp,
					 EdxlOpListType dxl_op_list_type = EdxloplistGeneral);

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

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
	static CDXLScalarOpList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarOpList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarOpList *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarOpList_H

// EOF
