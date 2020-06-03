//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CColumnDescriptor.cpp
//
//	@doc:
//		Column abstraction for tables, functions, external tables etc.
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/metadata/CColumnDescriptor.h"
#include "naucrates/md/CMDIdGPDB.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CColumnDescriptor::CColumnDescriptor
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CColumnDescriptor::CColumnDescriptor
	(
	CMemoryPool *mp,
	const IMDType *pmdtype,
	INT type_modifier,
	const CName &name,
	INT attno,
	BOOL is_nullable,
	ULONG ulWidth
	)
	:
	m_pmdtype(pmdtype),
	m_type_modifier(type_modifier),
	m_name(mp, name),
	m_iAttno(attno),
	m_is_nullable(is_nullable),
	m_width(ulWidth)
{
	GPOS_ASSERT(NULL != pmdtype);
	GPOS_ASSERT(pmdtype->MDId()->IsValid());

	if (m_pmdtype->IsFixedLength())
	{
		ulWidth = m_pmdtype->Length();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CColumnDescriptor::~CColumnDescriptor
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CColumnDescriptor::~CColumnDescriptor()
{}

//---------------------------------------------------------------------------
//	@function:
//		CColumnDescriptor::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CColumnDescriptor::OsPrint
	(
	IOstream &os
	)
	const
{
	return m_name.OsPrint(os);
}


// EOF

