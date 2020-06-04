//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBScalarOp.h
//
//	@doc:
//		SAX parse handler class for GPDB scalar operator metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDGPDBScalarOp_H
#define GPDXL_CParseHandlerMDGPDBScalarOp_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"

namespace gpdxl
{
	using namespace gpos;

	XERCES_CPP_NAMESPACE_USE
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerMDGPDBScalarOp
	//
	//	@doc:
	//		Parse handler for GPDB scalar operator metadata
	//
	//---------------------------------------------------------------------------
	class CParseHandlerMDGPDBScalarOp : public CParseHandlerMetadataObject
	{
		private:
			// id and version 
			IMDId *m_mdid;
			
			// name
			CMDName *m_mdname;
			
			// type of left operand
			IMDId *m_mdid_type_left;
			
			// type of right operand
			IMDId *m_mdid_type_right;

			// type of result operand
			IMDId *m_mdid_type_result;
			
			// id of function which implements the operator
			IMDId *m_func_mdid;
			
			// id of commute operator
			IMDId *m_mdid_commute_opr;
			
			// id of inverse operator
			IMDId *m_mdid_inverse_opr;
			
			// comparison type
			IMDType::ECmpType m_comparision_type;
			
			// does operator return NULL on NULL input?
			BOOL m_returns_null_on_null_input;

			IMDId *m_mdid_hash_opfamily;
			IMDId *m_mdid_legacy_hash_opfamily;

			// preserves NDVs of inputs
			BOOL m_is_ndv_preserving;

			// private copy ctor
			CParseHandlerMDGPDBScalarOp(const CParseHandlerMDGPDBScalarOp &);
			
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

			// is this a supported child elem of the scalar op
			BOOL IsSupportedChildElem(const XMLCh* const xml_str);
						
		public:
			// ctor
			CParseHandlerMDGPDBScalarOp
				(
				CMemoryPool *mp,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);			
	};
}

#endif // !GPDXL_CParseHandlerMDGPDBScalarOp_H

// EOF
