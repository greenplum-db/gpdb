//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	Class for representing DXL Part list null test expressions
//	These expressions indicate whether the list values of a part
//	contain NULL value or not
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarPartListNullTest_H
#define GPDXL_CDXLScalarPartListNullTest_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
class CDXLScalarPartListNullTest : public CDXLScalar
{
private:
	// partitioning level
	ULONG m_partitioning_level;

	// Null Test type (true for 'is null', false for 'is not null')
	BOOL m_is_null;

public:
	CDXLScalarPartListNullTest(const CDXLScalarPartListNullTest &) = delete;

	// ctor
	CDXLScalarPartListNullTest(CMemoryPool *mp, ULONG partitioning_level,
							   BOOL is_null);

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// partitioning level
	ULONG GetPartitioningLevel() const;

	// Null Test type (true for 'is null', false for 'is not null')
	BOOL IsNull() const;

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
	static CDXLScalarPartListNullTest *Cast(CDXLOperator *dxl_op);
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarPartListNullTest_H

// EOF
