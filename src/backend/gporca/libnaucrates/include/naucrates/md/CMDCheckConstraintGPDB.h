//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDCheckConstraintGPDB.h
//
//	@doc:
//		Class representing a GPDB check constraint in a metadata cache relation
//---------------------------------------------------------------------------

#ifndef GPMD_CMDCheckConstraintGPDB_H
#define GPMD_CMDCheckConstraintGPDB_H

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/IMDCheckConstraint.h"
#include "naucrates/md/CMDName.h"

// fwd decl
namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CMDCheckConstraintGPDB
//
//	@doc:
//		Class representing a GPDB check constraint in a metadata cache relation
//
//---------------------------------------------------------------------------
class CMDCheckConstraintGPDB : public IMDCheckConstraint
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// check constraint mdid
	IMDId *m_mdid;

	// check constraint name
	CMDName *m_mdname;

	// relation mdid
	IMDId *m_rel_mdid;

	// the DXL representation of the check constraint
	CDXLNode *m_dxl_node;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

public:
	// ctor
	CMDCheckConstraintGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
						   IMDId *rel_mdid, CDXLNode *dxlnode);

	// dtor
	virtual ~CMDCheckConstraintGPDB();

	// check constraint mdid
	virtual IMDId *
	MDId() const
	{
		return m_mdid;
	}

	// check constraint name
	virtual CMDName
	Mdname() const
	{
		return *m_mdname;
	}

	// mdid of the relation
	virtual IMDId *
	GetRelMdId() const
	{
		return m_rel_mdid;
	}

	// DXL string for check constraint
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	// the scalar expression of the check constraint
	virtual CExpression *GetCheckConstraintExpr(
		CMemoryPool *mp, CMDAccessor *md_accessor,
		CColRefArray *colref_array) const;

	// serialize MD check constraint in DXL format given a serializer object
	virtual void Serialize(gpdxl::CXMLSerializer *) const;

#ifdef GPOS_DEBUG
	// debug print of the MD check constraint
	virtual void DebugPrint(IOstream &os) const;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDCheckConstraintGPDB_H

// EOF
