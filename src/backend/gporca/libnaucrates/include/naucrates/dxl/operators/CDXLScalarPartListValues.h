//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	Class for representing DXL Part List Values expressions
//	These expressions indicate the constant values for the list partition
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarPartListValues_H
#define GPDXL_CDXLScalarPartListValues_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
class CDXLScalarPartListValues : public CDXLScalar
{
private:
	// partitioning level
	ULONG m_partitioning_level;

	// result type
	IMDId *m_result_type_mdid;

	// element type
	IMDId *m_elem_type_mdid;

public:
	CDXLScalarPartListValues(const CDXLScalarPartListValues &) = delete;

	// ctor
	CDXLScalarPartListValues(CMemoryPool *mp, ULONG partitioning_level,
							 IMDId *result_type_mdid, IMDId *elem_type_mdid);

	// dtor
	~CDXLScalarPartListValues() override;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// partitioning level
	ULONG GetPartitioningLevel() const;

	// result type
	IMDId *GetResultTypeMdId() const;

	// element type
	IMDId *GetElemTypeMdId() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLScalarPartListValues *Cast(CDXLOperator *dxl_op);
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarPartListValues_H

// EOF
