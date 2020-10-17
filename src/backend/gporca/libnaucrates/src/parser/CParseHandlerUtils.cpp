//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerUtils.cpp
//
//	@doc:
//		Implementation of the helper methods for parse handler
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/statistics/IStatistics.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerUtils::SetProperties
//
//	@doc:
//		Parse and the set operator's costing and statistical properties
//
//---------------------------------------------------------------------------
void
CParseHandlerUtils::SetProperties(CDXLNode *dxlnode,
								  CParseHandlerProperties *prop_parse_handler)
{
	GPOS_ASSERT(NULL != prop_parse_handler->GetProperties());
	// set physical properties
	CDXLPhysicalProperties *dxl_properties =
		prop_parse_handler->GetProperties();
	dxl_properties->AddRef();
	dxlnode->SetProperties(dxl_properties);

	// set the statistical information
	CDXLStatsDerivedRelation *dxl_stats_derived_relation =
		prop_parse_handler->GetDxlStatsDrvdRelation();
	if (NULL != dxl_stats_derived_relation)
	{
		dxl_stats_derived_relation->AddRef();
		dxl_properties->SetStats(dxl_stats_derived_relation);
	}
}

// EOF
