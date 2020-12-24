//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerMDRelationCtas.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing CTAS
//		relation metadata.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDRelationCtas.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataColumns.h"
#include "naucrates/dxl/parser/CParseHandlerCtasStorageOptions.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataIdList.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/md/CMDRelationCtasGPDB.h"

#include "naucrates/dxl/operators/CDXLCtasStorageOptions.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelationCtas::CParseHandlerMDRelationCtas
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDRelationCtas::CParseHandlerMDRelationCtas(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMDRelation(mp, parse_handler_mgr, parse_handler_root),
	  m_vartypemod_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelationCtas::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRelationCtas::StartElement(const XMLCh *const,
										  const XMLCh *const element_local_name,
										  const XMLCh *const,
										  const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenRelationCTAS),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse main relation attributes: name, id, distribution policy and keys
	ParseRelationAttributes(attrs, EdxltokenRelation);

	GPOS_ASSERT(IMDId::EmdidGPDBCtas == m_mdid->MdidType());

	const XMLCh *xml_str_schema =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenSchema));
	if (NULL != xml_str_schema)
	{
		m_mdname_schema = CDXLUtils::CreateMDNameFromXMLChar(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_schema);
	}

	// parse whether relation is temporary
	m_is_temp_table = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenRelTemporary, EdxltokenRelation);

	// parse whether relation has oids
	const XMLCh *xml_str_has_oids =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenRelHasOids));
	if (NULL != xml_str_has_oids)
	{
		m_has_oids = CDXLOperatorFactory::ConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_has_oids,
			EdxltokenRelHasOids, EdxltokenRelation);
	}

	// parse storage type
	const XMLCh *xml_str_storage_type = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenRelStorageType, EdxltokenRelation);
	m_rel_storage_type =
		CDXLOperatorFactory::ParseRelationStorageType(xml_str_storage_type);

	// parse vartypemod
	const XMLCh *vartypemod_xml = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenVarTypeModList, EdxltokenRelation);
	m_vartypemod_array = CDXLOperatorFactory::ExtractIntsToIntArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), vartypemod_xml,
		EdxltokenVarTypeModList, EdxltokenRelation);

	// parse handler for distr opclasses
	CParseHandlerBase *opclasses_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(opclasses_parse_handler);

	// parse handler for distr opfamilies
	CParseHandlerBase *opfamilies_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(opfamilies_parse_handler);

	//parse handler for the storage options
	CParseHandlerBase *ctas_options_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenCTASOptions),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(ctas_options_parse_handler);

	// parse handler for the columns
	CParseHandlerBase *columns_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataColumns),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(columns_parse_handler);

	// store parse handlers
	this->Append(columns_parse_handler);
	this->Append(ctas_options_parse_handler);
	this->Append(opfamilies_parse_handler);
	this->Append(opclasses_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelationCtas::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRelationCtas::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenRelationCTAS),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerMetadataColumns *md_cols_parse_handler =
		dynamic_cast<CParseHandlerMetadataColumns *>((*this)[0]);
	CParseHandlerCtasStorageOptions *ctas_options_parse_handler =
		dynamic_cast<CParseHandlerCtasStorageOptions *>((*this)[1]);
	CParseHandlerMetadataIdList *opfamilies_parse_handler =
		dynamic_cast<CParseHandlerMetadataIdList *>((*this)[2]);
	CParseHandlerMetadataIdList *opclasses_parse_handler =
		dynamic_cast<CParseHandlerMetadataIdList *>((*this)[3]);

	GPOS_ASSERT(NULL != md_cols_parse_handler->GetMdColArray());
	GPOS_ASSERT(NULL != ctas_options_parse_handler->GetDxlCtasStorageOption());
	GPOS_ASSERT(NULL != opfamilies_parse_handler->GetMdIdArray());
	GPOS_ASSERT(NULL != opclasses_parse_handler->GetMdIdArray());

	CMDColumnArray *md_col_array = md_cols_parse_handler->GetMdColArray();
	CDXLCtasStorageOptions *dxl_ctas_storage_options =
		ctas_options_parse_handler->GetDxlCtasStorageOption();

	md_col_array->AddRef();
	dxl_ctas_storage_options->AddRef();

	IMdIdArray *distr_opfamilies =
		dynamic_cast<CParseHandlerMetadataIdList *>(opfamilies_parse_handler)
			->GetMdIdArray();
	distr_opfamilies->AddRef();

	IMdIdArray *distr_opclasses =
		dynamic_cast<CParseHandlerMetadataIdList *>(opclasses_parse_handler)
			->GetMdIdArray();
	distr_opclasses->AddRef();


	m_imd_obj = GPOS_NEW(m_mp) CMDRelationCtasGPDB(
		m_mp, m_mdid, m_mdname_schema, m_mdname, m_is_temp_table, m_has_oids,
		m_rel_storage_type, m_rel_distr_policy, md_col_array, m_distr_col_array,
		distr_opfamilies, distr_opclasses, m_key_sets_arrays,
		dxl_ctas_storage_options, m_vartypemod_array);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
