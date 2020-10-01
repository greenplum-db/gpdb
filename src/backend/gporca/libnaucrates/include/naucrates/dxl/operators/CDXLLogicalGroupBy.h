//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalGroupBy.h
//
//	@doc:
//		Class for representing DXL logical group by operators
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLLogicalGroupBy_H
#define GPDXL_CDXLLogicalGroupBy_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLNode.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalGroupBy
//
//	@doc:
//		Class for representing DXL logical group by operators
//
//---------------------------------------------------------------------------
class CDXLLogicalGroupBy : public CDXLLogical
{
private:
	// grouping column ids
	ULongPtrArray *m_grouping_colid_array;

	// private copy ctor
	CDXLLogicalGroupBy(CDXLLogicalGroupBy &);

	// serialize output grouping columns indices in DXL
	void SerializeGrpColsToDXL(CXMLSerializer *) const;

public:
	// ctors
	explicit CDXLLogicalGroupBy(CMemoryPool *mp);
	CDXLLogicalGroupBy(CMemoryPool *mp, ULongPtrArray *pdrgpulGrpColIds);

	// dtor
	virtual ~CDXLLogicalGroupBy();

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;
	const ULongPtrArray *GetGroupingColidArray() const;

	// set grouping column indices
	void SetGroupingColumns(ULongPtrArray *);

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLLogicalGroupBy *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalGrpBy == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalGroupBy *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalGroupBy_H

// EOF
