//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	@filename:
//		CSerializableOptimizerConfig.h
//
//	@doc:
//		Serializable optimizer configuration object used to use for minidumping
//---------------------------------------------------------------------------
#ifndef GPOS_CSerializableOptimizerConfig_H
#define GPOS_CSerializableOptimizerConfig_H

#include "gpos/base.h"
#include "gpos/error/CSerializable.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLNode.h"

using namespace gpos;
using namespace gpdxl;

namespace gpopt
{
// fwd decl
class COptimizerConfig;

//---------------------------------------------------------------------------
//	@class:
//		CSerializableOptimizerConfig
//
//	@doc:
//		Serializable optimizer configuration object
//
//---------------------------------------------------------------------------
class CSerializableOptimizerConfig : public CSerializable
{
private:
	CMemoryPool *m_mp;

	// optimizer configurations
	const COptimizerConfig *m_optimizer_config;

public:
	CSerializableOptimizerConfig(const CSerializableOptimizerConfig &) = delete;

	// ctor
	CSerializableOptimizerConfig(CMemoryPool *mp,
								 const COptimizerConfig *optimizer_config);

	// dtor
	~CSerializableOptimizerConfig() override;

	// serialize object to passed stream
	void Serialize(COstream &oos) override;

};	// class CSerializableOptimizerConfig
}  // namespace gpopt

#endif	// !GPOS_CSerializableOptimizerConfig_H

// EOF
