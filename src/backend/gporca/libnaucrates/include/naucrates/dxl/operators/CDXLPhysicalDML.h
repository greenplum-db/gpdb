//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalDML.h
//
//	@doc:
//		Class for representing physical DML operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalDML_H
#define GPDXL_CDXLPhysicalDML_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
// fwd decl
class CDXLTableDescr;
class CDXLDirectDispatchInfo;

enum EdxlDmlType
{
	Edxldmlinsert,
	Edxldmldelete,
	Edxldmlupdate,
	EdxldmlSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalDML
//
//	@doc:
//		Class for representing physical DML operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalDML : public CDXLPhysical
{
private:
	// operator type
	const EdxlDmlType m_dxl_dml_type;

	// target table descriptor
	CDXLTableDescr *m_dxl_table_descr;

	// list of source column ids
	ULongPtrArray *m_src_colids_array;

	// action column id
	ULONG m_action_colid;

	// oid column id
	ULONG m_oid_colid;

	// ctid column id
	ULONG m_ctid_colid;

	// segmentid column id
	ULONG m_segid_colid;

	// should update preserve tuple oids
	BOOL m_preserve_oids;

	// tuple oid column id
	ULONG m_tuple_oid;

	// direct dispatch info for insert statements
	CDXLDirectDispatchInfo *m_direct_dispatch_info;

	// needs the data to be sorted or not
	BOOL m_input_sort_req;

	// private copy ctor
	CDXLPhysicalDML(const CDXLPhysicalDML &);

public:
	// ctor
	CDXLPhysicalDML(CMemoryPool *mp, const EdxlDmlType dxl_dml_type,
					CDXLTableDescr *table_descr,
					ULongPtrArray *src_colids_array, ULONG action_colid,
					ULONG oid_colid, ULONG ctid_colid, ULONG segid_colid,
					BOOL preserve_oids, ULONG tuple_oid,
					CDXLDirectDispatchInfo *dxl_direct_dispatch_info,
					BOOL input_sort_req);

	// dtor
	virtual ~CDXLPhysicalDML();

	// operator type
	Edxlopid GetDXLOperator() const;

	// operator name
	const CWStringConst *GetOpNameStr() const;

	// DML operator type
	EdxlDmlType
	GetDmlOpType() const
	{
		return m_dxl_dml_type;
	}

	// target table descriptor
	CDXLTableDescr *
	GetDXLTableDescr() const
	{
		return m_dxl_table_descr;
	}

	// source column ids
	ULongPtrArray *
	GetSrcColIdsArray() const
	{
		return m_src_colids_array;
	}

	// action column id
	ULONG
	ActionColId() const
	{
		return m_action_colid;
	}

	// oid column id
	ULONG
	OidColId() const
	{
		return m_oid_colid;
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

	// direct dispatch info
	CDXLDirectDispatchInfo *
	GetDXLDirectDispatchInfo() const
	{
		return m_direct_dispatch_info;
	}

	// needs the data to be sorted or not
	BOOL
	IsInputSortReq() const
	{
		return m_input_sort_req;
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
	static CDXLPhysicalDML *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalDML == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalDML *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalDML_H

// EOF
