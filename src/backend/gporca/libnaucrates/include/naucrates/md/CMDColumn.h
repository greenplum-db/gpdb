//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMDColumn.h
//
//	@doc:
//		Class for representing metadata about relation's columns.
//---------------------------------------------------------------------------



#ifndef GPMD_CDXLColumn_H
#define GPMD_CDXLColumn_H

#include "gpos/base.h"

#include "naucrates/md/CMDName.h"
#include "naucrates/md/IMDColumn.h"


// fwd decl
namespace gpdxl
{
class CDXLNode;
class CXMLSerializer;
}  // namespace gpdxl

namespace gpmd
{
//---------------------------------------------------------------------------
//	@class:
//		CMDColumn
//
//	@doc:
//		Class for representing metadata about relation's columns.
//
//---------------------------------------------------------------------------
class CMDColumn : public IMDColumn
{
private:
	// attribute name
	CMDName *m_mdname;

	// attribute number
	INT m_attno;

	// column type
	IMDId *m_mdid_type;

	INT m_type_modifier;

	// is NULL an allowed value for the attribute
	BOOL m_is_nullable;

	// is column dropped
	BOOL m_is_dropped;

	// length of the column
	ULONG m_length;

	// default value expression
	gpdxl::CDXLNode *m_dxl_default_val;

	// private copy ctor
	CMDColumn(const CMDColumn &);

public:
	// ctor
	CMDColumn(CMDName *mdname, INT attrnum, IMDId *mdid_type, INT type_modifier,
			  BOOL is_nullable, BOOL is_dropped,
			  gpdxl::CDXLNode *dxl_dafault_value,
			  ULONG length = gpos::ulong_max);

	// dtor
	virtual ~CMDColumn();

	// accessors
	virtual CMDName Mdname() const;

	// column type
	virtual IMDId *MdidType() const;

	virtual INT TypeModifier() const;

	// attribute number
	virtual INT AttrNum() const;

	// is this a system column
	virtual BOOL
	IsSystemColumn() const
	{
		return (0 > m_attno);
	}

	// length of the column
	ULONG
	Length() const
	{
		return m_length;
	}

	// is the column nullable
	virtual BOOL IsNullable() const;

	// is the column dropped
	virtual BOOL IsDropped() const;

	// serialize metadata object in DXL format given a serializer object
	virtual void Serialize(gpdxl::CXMLSerializer *) const;

#ifdef GPOS_DEBUG
	// debug print of the column
	virtual void DebugPrint(IOstream &os) const;
#endif
};

// array of metadata column descriptor
typedef CDynamicPtrArray<CMDColumn, CleanupRelease> CMDColumnArray;

}  // namespace gpmd

#endif	// !GPMD_CDXLColumn_H

// EOF
