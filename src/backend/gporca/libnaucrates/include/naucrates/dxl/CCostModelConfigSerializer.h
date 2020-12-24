//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.

#ifndef GPDXL_CCostModelConfigSerializer_H
#define GPDXL_CCostModelConfigSerializer_H

#include <gpopt/cost/ICostModel.h>
#include "gpos/memory/CMemoryPool.h"
#include "gpos/common/CAutoP.h"


namespace gpdxl
{
class CXMLSerializer;
class CCostModelConfigSerializer
{
private:
	const gpopt::ICostModel *m_cost_model;

public:
	CCostModelConfigSerializer(const gpopt::ICostModel *cost_model);

	void Serialize(CXMLSerializer &xml_serializer) const;
};
}  // namespace gpdxl
#endif
