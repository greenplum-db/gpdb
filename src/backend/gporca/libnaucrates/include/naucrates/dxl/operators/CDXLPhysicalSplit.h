//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalSplit.h
//
//	@doc:
//		Class for representing physical split operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalSplit_H
#define GPDXL_CDXLPhysicalSplit_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
// fwd decl
class CDXLTableDescr;

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalSplit
//
//	@doc:
//		Class for representing physical split operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalSplit : public CDXLPhysical
{
private:
	// list of deletion column ids
	ULongPtrArray *m_deletion_colid_array;

	// list of insertion column ids
	ULongPtrArray *m_insert_colid_array;

	// action column id
	ULONG m_action_colid;

	// ctid column id
	ULONG m_ctid_colid;

	// segmentid column id
	ULONG m_segid_colid;

public:
	CDXLPhysicalSplit(const CDXLPhysicalSplit &) = delete;

	// ctor
	CDXLPhysicalSplit(CMemoryPool *mp, ULongPtrArray *delete_colid_array,
					  ULongPtrArray *insert_colid_array, ULONG action_colid,
					  ULONG ctid_colid, ULONG segid_colid);

	// dtor
	~CDXLPhysicalSplit() override;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// deletion column ids
	ULongPtrArray *
	GetDeletionColIdArray() const
	{
		return m_deletion_colid_array;
	}

	// insertion column ids
	ULongPtrArray *
	GetInsertionColIdArray() const
	{
		return m_insert_colid_array;
	}

	// action column id
	ULONG
	ActionColId() const
	{
		return m_action_colid;
	}

	// ctid column id
	ULONG
	GetCtIdColId() const
	{
		return m_ctid_colid;
	}

	// segmentid column id
	ULONG
	GetSegmentIdColId() const
	{
		return m_segid_colid;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalSplit *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalSplit == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalSplit *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalSplit_H

// EOF
