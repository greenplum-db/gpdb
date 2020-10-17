//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLIndexDescr.h
//
//	@doc:
//		Class for representing index descriptors
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLIndexDescriptor_H
#define GPDXL_CDXLIndexDescriptor_H

#include "gpos/base.h"
#include "naucrates/md/CMDName.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLIndexDescr
//
//	@doc:
//		Class for representing index descriptors in a DXL index scan node.
//
//---------------------------------------------------------------------------
class CDXLIndexDescr : public CRefCount
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// id and version information for the table
	IMDId *m_mdid;

	// index name
	CMDName *m_mdname;

	// private copy ctor
	CDXLIndexDescr(const CDXLIndexDescr &);

public:
	// ctor
	CDXLIndexDescr(CMemoryPool *mp, IMDId *mdid, CMDName *mdname);

	// dtor
	virtual ~CDXLIndexDescr();

	// accessors
	const CMDName *MdName() const;
	IMDId *MDId() const;

	// serialize the operator to a DXL document
	void SerializeToDXL(CXMLSerializer *) const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLIndexDescriptor_H

// EOF
