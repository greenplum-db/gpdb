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
	const CWStringDynamic *m_dxl_str = nullptr;

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

public:
	CMDScCmpGPDB(const CMDScCmpGPDB &) = delete;

	// ctor
	CMDScCmpGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
				 IMDId *left_mdid, IMDId *right_mdid,
				 IMDType::ECmpType cmp_type, IMDId *mdid_op);

	// dtor
	~CMDScCmpGPDB() override;

	// accessors
	const CWStringDynamic *GetStrRepr() override;

	// copmarison object id
	IMDId *MDId() const override;

	// cast object name
	CMDName Mdname() const override;

	// left type
	IMDId *GetLeftMdid() const override;

	// right type
	IMDId *GetRightMdid() const override;

	// comparison type
	IMDType::ECmpType ParseCmpType() const override;

	// comparison operator id
	IMDId *MdIdOp() const override;

	// serialize object in DXL format
	void Serialize(gpdxl::CXMLSerializer *xml_serializer) const override;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDScCmpGPDB_H

// EOF
