//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerBase.h
//
//	@doc:
//		Base SAX parse handler class for DXL documents
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerBase_H
#define GPDXL_CParseHandlerBase_H

#include "gpos/base.h"
#include "naucrates/exception.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/CDXLUtils.h"

#include <xercesc/sax2/DefaultHandler.hpp>

namespace gpdxl
{
	using namespace gpos;

	// fwd decl
	class CParseHandlerManager;
	class CParseHandlerBase;
	
	// dynamic arrays of parse handlers
	typedef CDynamicPtrArray<CParseHandlerBase, CleanupDelete> CParseHandlerBaseArray;
	
	XERCES_CPP_NAMESPACE_USE

	// DXL parse handler type. Currently we only annotate the top-level parse handlers
	// for the top-level DXL elements such as Plan, Query and Metadata
	enum EDxlParseHandlerType
	{
		EdxlphOptConfig,
		EdxlphEnumeratorConfig,
		EdxlphStatisticsConfig,
		EdxlphCTEConfig,
		EdxlphHint,
		EdxlphWindowOids,
		EdxlphTraceFlags,
		EdxlphPlan,
		EdxlphQuery,
		EdxlphMetadata,
		EdxlphMetadataRequest,
		EdxlphStatistics,
		EdxlphSearchStrategy,
		EdxlphCostParams,
		EdxlphCostParam,
		EdxlphScalarExpr,
		EdxlphOther
	};
	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerBase
	//
	//	@doc:
	//		Base SAX parse handler class for parsing DXL documents.
	//		Implements Xerces required interface for SAX parse handlers.
	//
	//---------------------------------------------------------------------------
	class CParseHandlerBase : public DefaultHandler 
	{

		private:
		
			// private copy ctor
			CParseHandlerBase(const CParseHandlerBase&); 
			
			// array of parse handlers for child elements
		CParseHandlerBaseArray *m_parse_handler_base_array;
			
		protected:
			// memory pool to create DXL objects in
			CMemoryPool *m_mp;
			
			// manager for transitions between parse handlers
			CParseHandlerManager *m_parse_handler_mgr;

			// add child parse handler
			inline
			void Append
				(
				CParseHandlerBase *parse_handler_base
				)
			{
				GPOS_ASSERT(NULL != parse_handler_base);
				m_parse_handler_base_array->Append(parse_handler_base);
			};

			// number of children
			inline
			ULONG Length() const
			{
				return m_parse_handler_base_array->Size();
			}

			// shorthand to access children
			inline
			CParseHandlerBase *operator []
				(
				ULONG idx
				)
				const
			{
				return (*m_parse_handler_base_array)[idx];
			};
			
			// parse handler for root element
			CParseHandlerBase *m_parse_handler_root;
			
			// process the start of an element
			virtual 
			void StartElement
				(
				const XMLCh* const element_uri, 		// URI of element's namespace
				const XMLCh* const element_local_name,	// local part of element's name
				const XMLCh* const element_qname,		// element's qname
				const Attributes& attr				// element's attributes
				) = 0;
				
			// process the end of an element
			virtual 
			void EndElement
				(
				const XMLCh* const element_uri, 		// URI of element's namespace
				const XMLCh* const element_local_name,	// local part of element's name
				const XMLCh* const element_qname		// element's qname
				) = 0;

		public:
			// ctor
			CParseHandlerBase(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr, CParseHandlerBase *parse_handler_root);
			
			//dtor
			virtual
			~CParseHandlerBase();
			
			virtual 
			EDxlParseHandlerType GetParseHandlerType() const;

			// replaces a parse handler in the parse handler array with a new one
			void ReplaceParseHandler(CParseHandlerBase *parse_handler_base_old, CParseHandlerBase *parse_handler_base_new);
			
			// Xerces parse handler interface method to eceive notification of the beginning of an element.
			void startElement
				(
				const XMLCh* const element_uri, 		// URI of element's namespace
				const XMLCh* const element_local_name,	// local part of element's name
				const XMLCh* const element_qname,		// element's qname
				const Attributes& attr				// element's attributes
				);

			// Xerces parse handler interface method to eceive notification of the end of an element.
			void endElement
				(
				const XMLCh* const element_uri, 		// URI of element's namespace
				const XMLCh* const element_local_name,	// local part of element's name
				const XMLCh* const element_qname		// element's qname
				);
			
			// process a parsing ProcessError
			void error(const SAXParseException&);
	};
}

#endif // !GPDXL_CParseHandlerBase_H

// EOF
