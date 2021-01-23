//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDTriggerGPDB.h
//
//	@doc:
//		Implementation of GPDB-specific triggers in the metadata cache
//---------------------------------------------------------------------------

#ifndef GPMD_CMDTriggerGPDB_H
#define GPMD_CMDTriggerGPDB_H

#include "gpos/base.h"

#include "naucrates/md/IMDTrigger.h"

#define GPMD_TRIGGER_ROW 1
#define GPMD_TRIGGER_BEFORE 2
#define GPMD_TRIGGER_INSERT 4
#define GPMD_TRIGGER_DELETE 8
#define GPMD_TRIGGER_UPDATE 16

namespace gpmd
{
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CMDTriggerGPDB
//
//	@doc:
//		Implementation for GPDB-specific triggers in the metadata cache
//
//---------------------------------------------------------------------------
class CMDTriggerGPDB : public IMDTrigger
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// trigger id
	IMDId *m_mdid;

	// trigger name
	CMDName *m_mdname;

	// relation id
	IMDId *m_rel_mdid;

	// function id
	IMDId *m_func_mdid;

	// trigger type
	INT m_type;

	// is trigger enabled
	BOOL m_is_enabled;

public:
	CMDTriggerGPDB(const CMDTriggerGPDB &) = delete;

	// ctor
	CMDTriggerGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
				   IMDId *rel_mdid, IMDId *mdid_func, INT type,
				   BOOL is_enabled);

	// dtor
	~CMDTriggerGPDB() override;

	// accessors
	const CWStringDynamic *
	GetStrRepr() const override
	{
		return m_dxl_str;
	}

	// trigger id
	IMDId *
	MDId() const override
	{
		return m_mdid;
	}

	// trigger name
	CMDName
	Mdname() const override
	{
		return *m_mdname;
	}

	// relation mdid
	IMDId *
	GetRelMdId() const override
	{
		return m_rel_mdid;
	}

	// function mdid
	IMDId *
	FuncMdId() const override
	{
		return m_func_mdid;
	}

	// does trigger execute on a row-level
	BOOL ExecutesOnRowLevel() const override;

	// is this a before trigger
	BOOL IsBefore() const override;

	// is this an insert trigger
	BOOL IsInsert() const override;

	// is this a delete trigger
	BOOL IsDelete() const override;

	// is this an update trigger
	BOOL IsUpdate() const override;

	// is trigger enabled
	BOOL
	IsEnabled() const override
	{
		return m_is_enabled;
	}

	// serialize object in DXL format
	void Serialize(gpdxl::CXMLSerializer *xml_serializer) const override;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDTriggerGPDB_H

// EOF
