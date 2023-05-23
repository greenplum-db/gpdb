//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CMDCastGPDB.h
//
//	@doc:
//		Implementation of GPDB-specific cast functions in the metadata cache
//---------------------------------------------------------------------------


#ifndef GPMD_CMDCastGPDB_H
#define GPMD_CMDCastGPDB_H

#include "gpos/base.h"

#include "naucrates/md/IMDCast.h"

namespace gpmd
{
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CMDCastGPDB
//
//	@doc:
//		Implementation for GPDB-specific cast functions in the metadata cache
//
//---------------------------------------------------------------------------
class CMDCastGPDB : public IMDCast
{
private:
protected:
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str = nullptr;

	// func id
	IMDId *m_mdid;

	// func name
	CMDName *m_mdname;

	// source type
	IMDId *m_mdid_src;

	// destination type
	IMDId *m_mdid_dest;

	// is cast between binary coercible types, i.e. the types are binary compatible
	BOOL m_is_binary_coercible;

	// cast func id
	IMDId *m_mdid_cast_func;

	// coercion path type
	EmdCoercepathType m_path_type;

public:
	CMDCastGPDB(const CMDCastGPDB &) = delete;

	// ctor
	CMDCastGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname, IMDId *mdid_src,
				IMDId *mdid_dest, BOOL is_binary_coercible,
				IMDId *mdid_cast_func, EmdCoercepathType path_type = EmdtNone);

	// dtor
	~CMDCastGPDB() override;

	// accessors
	const CWStringDynamic *GetStrRepr() override;

	// cast object id
	IMDId *MDId() const override;

	// cast object name
	CMDName Mdname() const override;

	// source type
	IMDId *MdidSrc() const override;

	// destination type
	IMDId *MdidDest() const override;

	// is this a cast between binary coeercible types, i.e. the types are binary compatible
	BOOL IsBinaryCoercible() const override;

	// return the coercion path type
	EmdCoercepathType GetMDPathType() const override;

	// cast function id
	IMDId *GetCastFuncMdId() const override;

	// serialize object in DXL format
	void Serialize(gpdxl::CXMLSerializer *xml_serializer) const override;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDCastGPDB_H

// EOF
