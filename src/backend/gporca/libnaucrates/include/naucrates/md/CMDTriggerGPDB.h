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

	// private copy ctor
	CMDTriggerGPDB(const CMDTriggerGPDB &);

public:
	// ctor
	CMDTriggerGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
				   IMDId *rel_mdid, IMDId *mdid_func, INT type,
				   BOOL is_enabled);

	// dtor
	~CMDTriggerGPDB();

	// accessors
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	// trigger id
	virtual IMDId *
	MDId() const
	{
		return m_mdid;
	}

	// trigger name
	virtual CMDName
	Mdname() const
	{
		return *m_mdname;
	}

	// relation mdid
	virtual IMDId *
	GetRelMdId() const
	{
		return m_rel_mdid;
	}

	// function mdid
	virtual IMDId *
	FuncMdId() const
	{
		return m_func_mdid;
	}

	// does trigger execute on a row-level
	virtual BOOL ExecutesOnRowLevel() const;

	// is this a before trigger
	virtual BOOL IsBefore() const;

	// is this an insert trigger
	virtual BOOL IsInsert() const;

	// is this a delete trigger
	virtual BOOL IsDelete() const;

	// is this an update trigger
	virtual BOOL IsUpdate() const;

	// is trigger enabled
	virtual BOOL
	IsEnabled() const
	{
		return m_is_enabled;
	}

	// serialize object in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	virtual void DebugPrint(IOstream &os) const;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDTriggerGPDB_H

// EOF
