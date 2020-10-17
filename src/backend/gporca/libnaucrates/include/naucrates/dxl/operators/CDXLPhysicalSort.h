//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalSort.h
//
//	@doc:
//		Class for representing DXL sort operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalSort_H
#define GPDXL_CDXLPhysicalSort_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
// indices of sort node elements in the children array
enum Edxlsort
{
	EdxlsortIndexProjList = 0,
	EdxlsortIndexFilter,
	EdxlsortIndexSortColList,
	EdxlsortIndexLimitCount,
	EdxlsortIndexLimitOffset,
	EdxlsortIndexChild,
	EdxlsortIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalSort
//
//	@doc:
//		Class for representing DXL sort operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalSort : public CDXLPhysical
{
private:
	// private copy ctor
	CDXLPhysicalSort(const CDXLPhysicalSort &);

	// whether sort discards duplicates
	BOOL m_discard_duplicates;


public:
	// ctor/dtor
	CDXLPhysicalSort(CMemoryPool *mp, BOOL discard_duplicates);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;
	BOOL FDiscardDuplicates() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalSort *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalSort == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalSort *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalSort_H

// EOF
