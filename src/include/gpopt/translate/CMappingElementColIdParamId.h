//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMappingElementColIdParamId.h
//
//	@doc:
//		Wrapper class providing functions for the mapping element between ColId
//		and ParamId during DXL->PlStmt translation
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CMappingElementColIdParamId_H
#define GPDXL_CMappingElementColIdParamId_H

#include "gpos/base.h"

#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
//---------------------------------------------------------------------------
//	@class:
//		CMappingElementColIdParamId
//
//	@doc:
//		Wrapper class providing functions for the mapping element between
//		ColId and ParamId during DXL->PlStmt translation
//
//---------------------------------------------------------------------------
class CMappingElementColIdParamId : public CRefCount
{
private:
	// column identifier that is used as the key
	ULONG m_colid;

	// param identifier
	ULONG m_paramid;

	// param type
	IMDId *m_mdid;

	INT m_type_modifier;

public:
	// ctors and dtor
	CMappingElementColIdParamId(ULONG colid, ULONG paramid, IMDId *mdid,
								INT type_modifier);

	~CMappingElementColIdParamId() override = default;

	// return the ColId
	ULONG
	GetColId() const
	{
		return m_colid;
	}

	// return the ParamId
	ULONG
	ParamId() const
	{
		return m_paramid;
	}

	// return the type
	IMDId *
	MdidType() const
	{
		return m_mdid;
	}

	INT
	TypeModifier() const
	{
		return m_type_modifier;
	}
};
}  // namespace gpdxl

#endif	// GPDXL_CMappingElementColIdParamId_H

// EOF
