//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc
//
//	@filename:
//		CParseHandlerPhysicalCTAS.h
//
//	@doc:
//		Parse handler for parsing a physical CTAS operator
//		
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerPhysicalCTAS_H
#define GPDXL_CParseHandlerPhysicalCTAS_H

#include "gpos/base.h"
#include "naucrates/md/IMDRelation.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"


namespace gpdxl
{
	using namespace gpos;

	XERCES_CPP_NAMESPACE_USE

	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerPhysicalCTAS
	//
	//	@doc:
	//		Parse handler for parsing a physical CTAS operator
	//
	//---------------------------------------------------------------------------
	class CParseHandlerPhysicalCTAS : public CParseHandlerPhysicalOp
	{
		private:
			
			// schema name
			CMDName *m_mdname_schema;
		
			// table name
			CMDName *m_mdname;
	
			// list of distribution column positions		
			ULongPtrArray *m_distr_column_pos_array;
			
			// list of source column ids		
			ULongPtrArray *m_src_colids_array;

			// list of vartypmod
			IntPtrArray *m_vartypemod_array;
			
			// is this a temporary table
			BOOL m_is_temp_table;
			
			// does table have oids
			BOOL m_has_oids; 
			
			// distribution policy
			IMDRelation::Ereldistrpolicy m_rel_distr_policy;
			
			// storage type
			IMDRelation::Erelstoragetype m_rel_storage_type;
		
			// private copy ctor
			CParseHandlerPhysicalCTAS(const CParseHandlerPhysicalCTAS &);

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
			CParseHandlerPhysicalCTAS
				(
				CMemoryPool *mp,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
	};
}

#endif // !GPDXL_CParseHandlerPhysicalCTAS_H

// EOF
