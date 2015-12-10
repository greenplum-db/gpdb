//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMappingParamIdScalarId.cpp
//
//	@doc:
//		Implementation of param id -> scalar id mapping class
//
//	@test:
//
//
//---------------------------------------------------------------------------
#include "postgres.h"
#include "gpopt/translate/CMappingParamIdScalarId.h"
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CMappingParamIdScalarId::CMappingParamIdScalarId
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CMappingParamIdScalarId::CMappingParamIdScalarId
	(
	IMemoryPool *pmp
	)
	:
	m_pmp(pmp)
{
	m_phmps = GPOS_NEW(m_pmp) HMParamScalar(m_pmp);
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingParamIdScalarId::Pscid
//
//	@doc:
//		Lookup scalar ident associated with a given param id
//
//---------------------------------------------------------------------------
CDXLScalarIdent *
CMappingParamIdScalarId::Pscid
	(
	ULONG ulParamId
	)
	const
{
	return m_phmps->PtLookup(&ulParamId);
}

//---------------------------------------------------------------------------
//	@function:
//		CMappingParamIdScalarId::FInsertMapping
//
//	@doc:
//		Insert a (col id, scalarident) mapping
//
//---------------------------------------------------------------------------
BOOL
CMappingParamIdScalarId::FInsertMapping
	(
	ULONG ulParamId,
	CDXLScalarIdent *pscid
	)
{
	// copy key
	ULONG *pulKey = GPOS_NEW(m_pmp) ULONG(ulParamId);

	// insert mapping in the hash map
	return m_phmps->FInsert(pulKey, pscid);
}

// EOF
