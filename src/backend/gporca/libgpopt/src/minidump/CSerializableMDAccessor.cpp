//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSerializableMDAccessor.cpp
//
//	@doc:
//		Wrapper for serializing MD objects
//---------------------------------------------------------------------------

#include "gpopt/minidump/CSerializableMDAccessor.h"

#include "gpos/base.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/task/CTask.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/xml/CDXLSections.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CSerializableMDAccessor::CSerializableMDAccessor
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CSerializableMDAccessor::CSerializableMDAccessor(CMDAccessor *md_accessor)
	: CSerializable(), m_pmda(md_accessor)
{
	GPOS_ASSERT(NULL != md_accessor);
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializableMDAccessor::SerializeHeader
//
//	@doc:
//		Serialize header into provided stream
//
//---------------------------------------------------------------------------
void
CSerializableMDAccessor::SerializeHeader(COstream &oos)
{
	oos << CDXLSections::m_wszMetadataHeaderPrefix;

	m_pmda->SerializeSysid(oos);

	// serialize header suffix ">"
	oos << CDXLSections::m_wszMetadataHeaderSuffix;
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializableMDAccessor::SerializeFooter
//
//	@doc:
//		Serialize footer into provided stream
//
//---------------------------------------------------------------------------
void
CSerializableMDAccessor::SerializeFooter(COstream &oos)
{
	oos << CDXLSections::m_wszMetadataFooter;
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializableMDAccessor::Serialize
//
//	@doc:
//		Serialize contents into provided stream
//
//---------------------------------------------------------------------------
void
CSerializableMDAccessor::Serialize(COstream &oos)
{
	SerializeHeader(oos);
	m_pmda->Serialize(oos);
	SerializeFooter(oos);
}

// EOF
