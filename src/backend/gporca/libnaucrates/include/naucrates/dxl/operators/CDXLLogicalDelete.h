//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalDelete.h
//
//	@doc:
//		Class for representing logical delete operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalDelete_H
#define GPDXL_CDXLLogicalDelete_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogical.h"

namespace gpdxl
{
using namespace gpmd;

// fwd decl
class CDXLTableDescr;

//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalDelete
//
//	@doc:
//		Class for representing logical delete operator
//
//---------------------------------------------------------------------------
class CDXLLogicalDelete : public CDXLLogical
{
private:
	// target table descriptor
	CDXLTableDescr *m_dxl_table_descr;

	// ctid column id
	ULONG m_ctid_colid;

	// segmentId column id
	ULONG m_segid_colid;

	// list of deletion column ids
	ULongPtrArray *m_deletion_colid_array;

	// private copy ctor
	CDXLLogicalDelete(const CDXLLogicalDelete &);

public:
	// ctor
	CDXLLogicalDelete(CMemoryPool *mp, CDXLTableDescr *table_descr,
					  ULONG ctid_colid, ULONG segid_colid,
					  ULongPtrArray *delete_colid_array);

	// dtor
	virtual ~CDXLLogicalDelete();

	// operator type
	Edxlopid GetDXLOperator() const;

	// operator name
	const CWStringConst *GetOpNameStr() const;

	// target table descriptor
	CDXLTableDescr *
	GetDXLTableDescr() const
	{
		return m_dxl_table_descr;
	}

	// ctid column
	ULONG
	GetCtIdColId() const
	{
		return m_ctid_colid;
	}

	// segment id column
	ULONG
	GetSegmentIdColId() const
	{
		return m_segid_colid;
	}

	// deletion column ids
	ULongPtrArray *
	GetDeletionColIdArray() const
	{
		return m_deletion_colid_array;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLLogicalDelete *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalDelete == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalDelete *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLLogicalDelete_H

// EOF
