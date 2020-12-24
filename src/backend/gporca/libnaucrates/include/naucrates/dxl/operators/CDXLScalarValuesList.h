//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarValuesList.h
//
//	@doc:
//		Class for representing DXL value list operator.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarValuesList_H
#define GPDXL_CDXLScalarValuesList_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;


// class for representing DXL value list operator
class CDXLScalarValuesList : public CDXLScalar
{
private:
public:
	CDXLScalarValuesList(CDXLScalarValuesList &) = delete;

	// ctor
	CDXLScalarValuesList(CMemoryPool *mp);

	// dtor
	~CDXLScalarValuesList() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLScalarValuesList *Cast(CDXLOperator *dxl_op);

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor * /*md_accessor*/) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarValuesList_H

// EOF
