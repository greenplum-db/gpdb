//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CMDArrayCoerceCastGPDB.h
//
//	@doc:
//		Implementation of GPDB-specific array coerce cast functions in the
//		metadata cache
//---------------------------------------------------------------------------


#ifndef GPMD_CMDArrayCoerceCastGPDB_H
#define GPMD_CMDArrayCoerceCastGPDB_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/CMDCastGPDB.h"

namespace gpmd
{
using namespace gpdxl;

class CMDArrayCoerceCastGPDB : public CMDCastGPDB
{
private:
	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// type mod
	INT m_type_modifier;

	// is explicit
	BOOL m_is_explicit;

	// CoercionForm
	EdxlCoercionForm m_dxl_coerce_format;

	// location
	INT m_location;

	// private copy ctor
	CMDArrayCoerceCastGPDB(const CMDArrayCoerceCastGPDB &);

public:
	// ctor
	CMDArrayCoerceCastGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
						   IMDId *mdid_src, IMDId *mdid_dest,
						   BOOL is_binary_coercible, IMDId *mdid_cast_func,
						   EmdCoercepathType path_type, INT type_modifier,
						   BOOL is_explicit, EdxlCoercionForm dxl_coerce_format,
						   INT location);

	// dtor
	virtual ~CMDArrayCoerceCastGPDB();

	// accessors
	virtual const CWStringDynamic *
	Pstr() const
	{
		return m_dxl_str;
	}

	// return type modifier
	virtual INT TypeModifier() const;

	virtual BOOL IsExplicit() const;

	// return coercion form
	virtual EdxlCoercionForm GetCoercionForm() const;

	// return token location
	virtual INT Location() const;

	// serialize object in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	virtual void DebugPrint(IOstream &os) const;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDArrayCoerceCastGPDB_H

// EOF
