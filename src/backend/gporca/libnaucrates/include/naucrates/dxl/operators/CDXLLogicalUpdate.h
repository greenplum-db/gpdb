//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalUpdate.h
//
//	@doc:
//		Class for representing logical update operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalUpdate_H
#define GPDXL_CDXLLogicalUpdate_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLLogical.h"

namespace gpdxl
{
using namespace gpmd;

// fwd decl
class CDXLTableDescr;

//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalUpdate
//
//	@doc:
//		Class for representing logical update operator
//
//---------------------------------------------------------------------------
class CDXLLogicalUpdate : public CDXLLogical
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

	// list of insertion column ids
	ULongPtrArray *m_insert_colid_array;

	// should update preserve tuple oids
	BOOL m_preserve_oids;

	// tuple oid column id
	ULONG m_tuple_oid;

	// private copy ctor
	CDXLLogicalUpdate(const CDXLLogicalUpdate &);

public:
	// ctor
	CDXLLogicalUpdate(CMemoryPool *mp, CDXLTableDescr *table_descr,
					  ULONG ctid_colid, ULONG segid_colid,
					  ULongPtrArray *delete_colid_array,
					  ULongPtrArray *insert_colid_array, BOOL preserve_oids,
					  ULONG tuple_oid);

	// dtor
	virtual ~CDXLLogicalUpdate();

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
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLLogicalUpdate *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalUpdate == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalUpdate *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLLogicalUpdate_H

// EOF
