//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSwitch.h
//
//	@doc:
//
//		SAX parse handler class for parsing Switch operator
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerScalarSwitch_H
#define GPDXL_CParseHandlerScalarSwitch_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/operators/CDXLScalarSwitch.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSwitch
//
//	@doc:
//		Parse handler for parsing a Switch operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSwitch : public CParseHandlerScalarOp
{
private:
	// return type
	IMDId *m_mdid_type;

	// was the arg child seen
	BOOL m_arg_processed;

	// was the default value seen
	BOOL m_default_val_processed;

	// private copy ctor
	CParseHandlerScalarSwitch(const CParseHandlerScalarSwitch &);

	// process the start of an element
	void StartElement(const XMLCh *const element_uri,
					  const XMLCh *const element_local_name,
					  const XMLCh *const element_qname, const Attributes &attr);

	// process the end of an element
	void EndElement(const XMLCh *const element_uri,
					const XMLCh *const element_local_name,
					const XMLCh *const element_qname);

public:
	// ctor
	CParseHandlerScalarSwitch(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarSwitch_H

//EOF
