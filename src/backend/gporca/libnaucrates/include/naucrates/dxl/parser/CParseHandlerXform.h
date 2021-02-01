//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerXform.h
//
//	@doc:
//		SAX parse handler class for parsing xform nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerXform_H
#define GPDXL_CParseHandlerXform_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"

// forward declarations
namespace gpopt
{
class CXform;
}

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerXform
//
//	@doc:
//		Parse handler for parsing xform
//
//---------------------------------------------------------------------------
class CParseHandlerXform : public CParseHandlerBase
{
private:
	// xform referred to by XML node
	CXform *m_xform;

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerXform(const CParseHandlerXform &) = delete;

	// ctor
	CParseHandlerXform(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
					   CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerXform() override;

	// returns the root of constructed DXL plan
	CXform *
	GetXform()
	{
		return m_xform;
	}

	EDxlParseHandlerType
	GetParseHandlerType() const override
	{
		return EdxlphSearchStrategy;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerXform_H

// EOF
