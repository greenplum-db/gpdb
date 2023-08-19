//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CDXLScalarFieldSelect.cpp
//
//	@doc:
//		Implementation of DXL Scalar FIELDSELECT operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLSCALARFIELDSELECT_H
#define GPDXL_CDXLSCALARFIELDSELECT_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarAggref
//
//	@doc:
//		Class for representing DXL FIELDSELECT
//
//---------------------------------------------------------------------------
class CDXLScalarFieldSelect : public CDXLScalar
{
private:
	// type of the field
	IMDId *m_result_field_mdid;

	// collation OID of the field
	IMDId *m_result_coll_mdid;

	// output typmod (usually -1)
	INT m_output_type_mode;

	// attribute number of field to extract
	USINT m_field_num;

public:
	CDXLScalarFieldSelect(const CDXLScalarFieldSelect &) = delete;

	// ctor/dtor
	CDXLScalarFieldSelect(CMemoryPool *mp, IMDId *field_mdid, IMDId *coll_mdid,
						  INT mode_type, INT field_num);

	~CDXLScalarFieldSelect() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// DXL operator name
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// mdid of the field
	IMDId *GetDXLFieldMDId() const;

	// collation mdid of the field
	IMDId *GetDXLCollMDId() const;

	// output type mode
	INT GetDXLModeType() const;

	// attribute number of the field
	USINT GetDXLFieldNumber() const;

	// conversion function
	static CDXLScalarFieldSelect *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarFieldSelect == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarFieldSelect *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDB_CDXLSCALARFIELDSELECT_H

// EOF
