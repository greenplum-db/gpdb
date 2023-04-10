//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLTableDescr.h
//
//	@doc:
//		Class for representing table descriptors.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLTableDescriptor_H
#define GPDXL_CDXLTableDescriptor_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLColDescr.h"
#include "naucrates/md/CMDName.h"
#include "naucrates/md/IMDId.h"

// default value for m_assigned_query_id_for_target_rel - no assigned query for table descriptor
#define UNASSIGNED_QUERYID 0

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLTableDescr
//
//	@doc:
//		Class for representing table descriptors in a DXL tablescan node.
//
//---------------------------------------------------------------------------
class CDXLTableDescr : public CRefCount
{
private:
	// id and version information for the table
	IMDId *m_mdid;

	// table name
	CMDName *m_mdname;

	// list of column descriptors
	CDXLColDescrArray *m_dxl_column_descr_array;

	// id of user the table needs to be accessed with
	ULONG m_execute_as_user_id;

	// lock mode from the parser
	INT m_lockmode;

	// identifier of query to which current table belongs.
	// This field is used for assigning current table entry with
	// target one within DML operation. If descriptor doesn't point
	// to the target (result) relation it has value UNASSIGNED_QUERYID
	ULONG m_assigned_query_id_for_target_rel;

	void SerializeMDId(CXMLSerializer *xml_serializer) const;

public:
	CDXLTableDescr(const CDXLTableDescr &) = delete;

	// ctor/dtor
	CDXLTableDescr(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
				   ULONG ulExecuteAsUser, int lockmode,
				   ULONG assigned_query_id_for_target_rel = UNASSIGNED_QUERYID);

	~CDXLTableDescr() override;

	// setters
	void SetColumnDescriptors(CDXLColDescrArray *dxl_column_descr_array);

	void AddColumnDescr(CDXLColDescr *pdxlcd);

	// table name
	const CMDName *MdName() const;

	// table mdid
	IMDId *MDId() const;

	// table arity
	ULONG Arity() const;

	// user id
	ULONG GetExecuteAsUserId() const;

	// lock mode
	INT LockMode() const;

	// get the column descriptor at the given position
	const CDXLColDescr *GetColumnDescrAt(ULONG idx) const;

	// serialize to dxl format
	void SerializeToDXL(CXMLSerializer *xml_serializer) const;

	// get assigned query id for target relation
	ULONG GetAssignedQueryIdForTargetRel() const;
};
}  // namespace gpdxl


#endif	// !GPDXL_CDXLTableDescriptor_H

// EOF
