//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CMDIdCast.h
//
//	@doc:
//		Class for representing mdids of cast functions
//---------------------------------------------------------------------------

#ifndef GPMD_CMDIdCastFunc_H
#define GPMD_CMDIdCastFunc_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/string/CWStringConst.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/CSystemId.h"

namespace gpmd
{
using namespace gpos;


//---------------------------------------------------------------------------
//	@class:
//		CMDIdCast
//
//	@doc:
//		Class for representing ids of cast objects
//
//---------------------------------------------------------------------------
class CMDIdCast : public IMDId
{
private:
	// mdid of source type
	CMDIdGPDB *m_mdid_src;

	// mdid of destinatin type
	CMDIdGPDB *m_mdid_dest;


	// buffer for the serialized mdid
	WCHAR m_mdid_buffer[GPDXL_MDID_LENGTH];

	// string representation of the mdid
	mutable CWStringStatic m_str;

	// serialize mdid
	void Serialize() const;

public:
	CMDIdCast(const CMDIdCast &) = delete;

	// ctor
	CMDIdCast(CMDIdGPDB *mdid_src, CMDIdGPDB *mdid_dest);

	// dtor
	~CMDIdCast() override;

	EMDIdType
	MdidType() const override
	{
		return EmdidCastFunc;
	}

	// string representation of mdid
	const WCHAR *GetBuffer() const override;

	// source system id
	CSystemId
	Sysid() const override
	{
		return m_mdid_src->Sysid();
	}

	// source type id
	IMDId *MdidSrc() const;

	// destination type id
	IMDId *MdidDest() const;

	// equality check
	BOOL Equals(const IMDId *mdid) const override;

	// computes the hash value for the metadata id
	ULONG
	HashValue() const override
	{
		return gpos::CombineHashes(
			MdidType(), gpos::CombineHashes(m_mdid_src->HashValue(),
											m_mdid_dest->HashValue()));
	}

	// is the mdid valid
	BOOL
	IsValid() const override
	{
		return IMDId::IsValid(m_mdid_src) && IMDId::IsValid(m_mdid_dest);
	}

	// serialize mdid in DXL as the value of the specified attribute
	void Serialize(CXMLSerializer *xml_serializer,
				   const CWStringConst *pstrAttribute) const override;

	// debug print of the metadata id
	IOstream &OsPrint(IOstream &os) const override;

	// const converter
	static const CMDIdCast *
	CastMdid(const IMDId *mdid)
	{
		GPOS_ASSERT(nullptr != mdid && EmdidCastFunc == mdid->MdidType());

		return dynamic_cast<const CMDIdCast *>(mdid);
	}

	// non-const converter
	static CMDIdCast *
	CastMdid(IMDId *mdid)
	{
		GPOS_ASSERT(nullptr != mdid && EmdidCastFunc == mdid->MdidType());

		return dynamic_cast<CMDIdCast *>(mdid);
	}

	// make a copy in the given memory pool
	IMDId *
	Copy(CMemoryPool *mp) const override
	{
		CMDIdGPDB *mdid_src = CMDIdGPDB::CastMdid(m_mdid_src->Copy(mp));
		CMDIdGPDB *mdid_dest = CMDIdGPDB::CastMdid(m_mdid_dest->Copy(mp));
		return GPOS_NEW(mp) CMDIdCast(mdid_src, mdid_dest);
	}
};
}  // namespace gpmd

#endif	// !GPMD_CMDIdCastFunc_H

// EOF
