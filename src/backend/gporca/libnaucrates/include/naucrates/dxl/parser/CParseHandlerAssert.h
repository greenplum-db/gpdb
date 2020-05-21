//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerAssert.h
//
//	@doc:
//		SAX parse handler class for parsing assert operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerAssert_H
#define GPDXL_CParseHandlerAssert_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalAssert.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
	using namespace gpos;

	XERCES_CPP_NAMESPACE_USE
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerAssert
	//
	//	@doc:
	//		Parse handler for DXL physical assert 
	//
	//---------------------------------------------------------------------------
	class CParseHandlerAssert : public CParseHandlerPhysicalOp
	{
		private:
			
			// physical assert operator
			CDXLPhysicalAssert *m_dxl_op;
			
			// private copy ctor
			CParseHandlerAssert(const CParseHandlerAssert&);
			
			// process the start of an element
			void StartElement
				(
				const XMLCh* const element_uri, 		// URI of element's namespace
				const XMLCh* const element_local_name,	// local part of element's name
				const XMLCh* const element_qname,		// element's qname
				const Attributes& attr				// element's attributes
				);
				
			// process the end of an element
			void EndElement
				(
				const XMLCh* const element_uri, 		// URI of element's namespace
				const XMLCh* const element_local_name,	// local part of element's name
				const XMLCh* const element_qname		// element's qname
				);
			
		public:
			// ctor
			CParseHandlerAssert
				(
				CMemoryPool *mp,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
	};
}

#endif // !GPDXL_CParseHandlerAssert_H

// EOF
