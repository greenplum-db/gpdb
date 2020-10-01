//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CMDScCmpGPDB.h
//
//	@doc:
//		Implementation of GPDB-specific scalar comparison operators in the MD cache
//---------------------------------------------------------------------------


#ifndef GPMD_CMDScCmpGPDB_H
#define GPMD_CMDScCmpGPDB_H

#include "gpos/base.h"

#include "naucrates/md/IMDScCmp.h"

namespace gpmd
{
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CMDScCmpGPDB
//
//	@doc:
//		Implementation for GPDB-specific scalar comparison operators in the
//		MD cache
//
//---------------------------------------------------------------------------
class CMDScCmpGPDB : public IMDScCmp
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// object id
	IMDId *m_mdid;

	// operator name
	CMDName *m_mdname;

	// left type
	IMDId *m_mdid_left;

	// right type
	IMDId *m_mdid_right;

	// comparison type
	IMDType::ECmpType m_comparision_type;

	// comparison operator id
	IMDId *m_mdid_op;

	// private copy ctor
	CMDScCmpGPDB(const CMDScCmpGPDB &);

public:
	// ctor
	CMDScCmpGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
				 IMDId *left_mdid, IMDId *right_mdid,
				 IMDType::ECmpType cmp_type, IMDId *mdid_op);

	// dtor
	virtual ~CMDScCmpGPDB();

	// accessors
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	// copmarison object id
	virtual IMDId *MDId() const;

	// cast object name
	virtual CMDName Mdname() const;

	// left type
	virtual IMDId *GetLeftMdid() const;

	// right type
	virtual IMDId *GetRightMdid() const;

	// comparison type
	virtual IMDType::ECmpType ParseCmpType() const;

	// comparison operator id
	virtual IMDId *MdIdOp() const;

	// serialize object in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	virtual void DebugPrint(IOstream &os) const;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDScCmpGPDB_H

// EOF
