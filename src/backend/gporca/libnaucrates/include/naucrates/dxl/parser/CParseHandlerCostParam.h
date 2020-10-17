//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerCostParam.h
//
//	@doc:
//		SAX parse handler class for parsing cost model param.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerCostParam_H
#define GPDXL_CParseHandlerCostParam_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerCostParam
//
//	@doc:
//		Parse handler for parsing cost model param
//
//---------------------------------------------------------------------------
class CParseHandlerCostParam : public CParseHandlerBase
{
private:
	// param name
	CHAR *m_param_name;

	// param value
	CDouble m_value;

	// lower bound value
	CDouble m_lower_bound_val;

	// upper bound value
	CDouble m_upper_bound_val;

	// private copy ctor
	CParseHandlerCostParam(const CParseHandlerCostParam &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor
	CParseHandlerCostParam(CMemoryPool *mp,
						   CParseHandlerManager *parse_handler_mgr,
						   CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerCostParam();

	// return parsed param name
	CHAR *
	GetName() const
	{
		return m_param_name;
	}

	// return parsed param value
	CDouble
	Get() const
	{
		return m_value;
	}

	// return parsed param lower bound value
	CDouble
	GetLowerBoundVal() const
	{
		return m_lower_bound_val;
	}

	// return parsed param upper bound value
	CDouble
	GetUpperBoundVal() const
	{
		return m_upper_bound_val;
	}

	EDxlParseHandlerType
	GetParseHandlerType() const
	{
		return EdxlphCostParam;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerCostParam_H

// EOF
