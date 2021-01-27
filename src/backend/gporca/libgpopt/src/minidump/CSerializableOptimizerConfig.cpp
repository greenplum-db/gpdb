//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc..
//
//	@filename:
//		CSerializableOptimizerConfig.cpp
//
//	@doc:
//		Class for serializing optimizer config objects
//---------------------------------------------------------------------------

#include "gpopt/minidump/CSerializableOptimizerConfig.h"

#include "gpos/base.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/task/CTask.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CDXLSections.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;

// max length of the string representation of traceflag codes
#define GPOPT_MAX_TRACEFLAG_CODE_LENGTH 10

//---------------------------------------------------------------------------
//	@function:
//		CSerializableOptimizerConfig::CSerializableOptimizerConfig
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CSerializableOptimizerConfig::CSerializableOptimizerConfig(
	CMemoryPool *mp, const COptimizerConfig *optimizer_config)
	: CSerializable(), m_mp(mp), m_optimizer_config(optimizer_config)
{
	GPOS_ASSERT(NULL != optimizer_config);
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializableOptimizerConfig::~CSerializableOptimizerConfig
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CSerializableOptimizerConfig::~CSerializableOptimizerConfig()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializableOptimizerConfig::Serialize
//
//	@doc:
//		Serialize contents into provided stream
//
//---------------------------------------------------------------------------
void
CSerializableOptimizerConfig::Serialize(COstream &oos)
{
	CXMLSerializer xml_serializer(m_mp, oos, false /*Indent*/);

	// Copy traceflags from global state
	CBitSet *pbs = CTask::Self()->GetTaskCtxt()->copy_trace_flags(m_mp);
	m_optimizer_config->Serialize(m_mp, &xml_serializer, pbs);
	pbs->Release();
}

// EOF
