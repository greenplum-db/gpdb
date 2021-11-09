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

public:
	CMDColumn(const CMDColumn &) = delete;

	// ctor
	CMDColumn(CMDName *mdname, INT attrnum, IMDId *mdid_type, INT type_modifier,
			  BOOL is_nullable, BOOL is_dropped,
			  gpdxl::CDXLNode *dxl_dafault_value,
			  ULONG length = gpos::ulong_max);

	// dtor
	~CMDColumn() override;

	// accessors
	CMDName Mdname() const override;

	// column type
	IMDId *MdidType() const override;

	INT TypeModifier() const override;

	// attribute number
	INT AttrNum() const override;

	// is this a system column
	BOOL
	IsSystemColumn() const override
	{
		return (0 > m_attno);
	}

	// length of the column
	ULONG
	Length() const override
	{
		return m_length;
	}

	// is the column nullable
	BOOL IsNullable() const override;

	// is the column dropped
	BOOL IsDropped() const override;

	// serialize metadata object in DXL format given a serializer object
	virtual void Serialize(gpdxl::CXMLSerializer *) const;

#ifdef GPOS_DEBUG
	// debug print of the column
	void DebugPrint(IOstream &os) const override;
#endif
};

// array of metadata column descriptor
using CMDColumnArray = CDynamicPtrArray<CMDColumn, CleanupRelease>;

}  // namespace gpmd

#endif	// !GPMD_CDXLColumn_H

// EOF
