//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLColRef.h
//
//	@doc:
//		Class for representing column references.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLColRef_H
#define GPDXL_CDXLColRef_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"

#include "naucrates/md/CMDName.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;
using namespace gpos;

// fwd decl
class CXMLSerializer;
class CDXLColRef;

// arrays of column references
using CDXLColRefArray = CDynamicPtrArray<CDXLColRef, CleanupRelease>;

//---------------------------------------------------------------------------
//	@class:
//		CDXLColRef
//
//	@doc:
//		Class for representing references to columns in DXL trees
//
//---------------------------------------------------------------------------
class CDXLColRef : public CRefCount
{
private:
	// name
	CMDName *m_mdname;

	// id
	const ULONG m_id;

	// column type
	IMDId *m_mdid_type;

	// column type modifier
	INT m_iTypeModifer;

public:
	CDXLColRef(const CDXLColRef &) = delete;

	// ctor/dtor
	CDXLColRef(CMDName *mdname, ULONG id, IMDId *mdid_type, INT type_modifier);

	~CDXLColRef() override;

	// accessors
	const CMDName *MdName() const;

	IMDId *MdidType() const;

	INT TypeModifier() const;

	ULONG Id() const;
};
}  // namespace gpdxl



#endif	// !GPDXL_CDXLColRef_H

// EOF
