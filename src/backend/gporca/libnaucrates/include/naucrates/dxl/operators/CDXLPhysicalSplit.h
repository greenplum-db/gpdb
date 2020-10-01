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

	// should update preserve tuple oids
	BOOL m_preserve_oids;

	// tuple oid column id
	ULONG m_tuple_oid;

	// private copy ctor
	CDXLPhysicalSplit(const CDXLPhysicalSplit &);

public:
	// ctor
	CDXLPhysicalSplit(CMemoryPool *mp, ULongPtrArray *delete_colid_array,
					  ULongPtrArray *insert_colid_array, ULONG action_colid,
					  ULONG ctid_colid, ULONG segid_colid, BOOL preserve_oids,
					  ULONG tuple_oid);

	// dtor
	virtual ~CDXLPhysicalSplit();

	// operator type
	Edxlopid GetDXLOperator() const;

	// operator name
	const CWStringConst *GetOpNameStr() const;

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

	// does update preserve oids
	BOOL
	IsOidsPreserved() const
	{
		return m_preserve_oids;
	}

	// tuple oid column id
	ULONG
	GetTupleOid() const
	{
		return m_tuple_oid;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalSplit *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalSplit == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalSplit *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalSplit_H

// EOF
