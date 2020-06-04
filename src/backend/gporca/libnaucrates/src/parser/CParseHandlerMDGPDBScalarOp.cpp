//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBScalarOp.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing metadata for
//		GPDB scalar operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDGPDBScalarOp.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataIdList.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/md/CMDScalarOpGPDB.h"

using namespace gpdxl;
using namespace gpmd;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBScalarOp::CParseHandlerMDGPDBScalarOp
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerMDGPDBScalarOp::CParseHandlerMDGPDBScalarOp
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root),
	m_mdid(NULL),
	m_mdname(NULL),
	m_mdid_type_left(NULL),
	m_mdid_type_right(NULL),
	m_mdid_type_result(NULL),
	m_func_mdid(NULL),
	m_mdid_commute_opr(NULL),
	m_mdid_inverse_opr(NULL),
	m_comparision_type(IMDType::EcmptOther),
	m_returns_null_on_null_input(false),
	m_mdid_hash_opfamily(NULL),
	m_mdid_legacy_hash_opfamily(NULL),
	m_is_ndv_preserving(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBScalarOp::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBScalarOp::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes& attrs
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOp), element_local_name))
	{
		// parse operator name
		const XMLCh *xml_str_op_name = CDXLOperatorFactory::ExtractAttrValue
															(
															attrs,
															EdxltokenName,
															EdxltokenGPDBScalarOp
															);

		CWStringDynamic *str_opname = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_op_name);
		
		// create a copy of the string in the CMDName constructor
		m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_opname);
		
		GPOS_DELETE(str_opname);

		// parse metadata id info
		m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
										(
										m_parse_handler_mgr->GetDXLMemoryManager(),
										attrs,
										EdxltokenMdid,
										EdxltokenGPDBScalarOp
										);
		
		const XMLCh *xml_str_comp_type = CDXLOperatorFactory::ExtractAttrValue
									(
									attrs,
									EdxltokenGPDBScalarOpCmpType,
									EdxltokenGPDBScalarOp
									);

		m_comparision_type = CDXLOperatorFactory::ParseCmpType(xml_str_comp_type);

		// null-returning property is optional
		const XMLCh *xml_str_returns_null_on_null_input = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenReturnsNullOnNullInput));
		if (NULL != xml_str_returns_null_on_null_input)
		{
			m_returns_null_on_null_input = CDXLOperatorFactory::ExtractConvertAttrValueToBool
								(
								m_parse_handler_mgr->GetDXLMemoryManager(),
								attrs,
								EdxltokenReturnsNullOnNullInput,
								EdxltokenGPDBScalarOp
								);
		}

		// ndv-preserving property is optional
		m_is_ndv_preserving = CDXLOperatorFactory::ExtractConvertAttrValueToBool
							(
							m_parse_handler_mgr->GetDXLMemoryManager(),
							attrs,
							EdxltokenIsNDVPreserving,
							EdxltokenGPDBScalarOp,
							true, // is optional
							false // default value
							);

	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpLeftTypeId), element_local_name))
	{
		// parse left operand's type
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_type_left = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
												(
												m_parse_handler_mgr->GetDXLMemoryManager(),
												attrs, 
												EdxltokenMdid,
												EdxltokenGPDBScalarOpLeftTypeId
												);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpRightTypeId), element_local_name))
	{
		// parse right operand's type
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_type_right = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													attrs,
													EdxltokenMdid,
													EdxltokenGPDBScalarOpRightTypeId
													);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpResultTypeId), element_local_name))
	{
		// parse result type
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_type_result = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													attrs,
													EdxltokenMdid,
													EdxltokenGPDBScalarOpResultTypeId
													);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpFuncId), element_local_name))
	{
		// parse op func id
		GPOS_ASSERT(NULL != m_mdname);

		m_func_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
												(
												m_parse_handler_mgr->GetDXLMemoryManager(),
												attrs, 
												EdxltokenMdid,
												EdxltokenGPDBScalarOpFuncId
												);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpCommOpId), element_local_name))
	{
		// parse commutator operator
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_commute_opr = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													attrs,
													EdxltokenMdid,
													EdxltokenGPDBScalarOpCommOpId
													);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpInverseOpId), element_local_name))
	{
		// parse inverse operator id
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_inverse_opr = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													attrs,
													EdxltokenMdid,
													EdxltokenGPDBScalarOpInverseOpId
													);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenOpfamilies), element_local_name))
	{
		// parse handler for operator class list
		CParseHandlerBase *opfamilies_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(opfamilies_list_parse_handler);
		this->Append(opfamilies_list_parse_handler);
		opfamilies_list_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpHashOpfamily), element_local_name))
	{
		// parse inverse operator id
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_hash_opfamily = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													attrs,
													EdxltokenMdid,
													EdxltokenGPDBScalarOpHashOpfamily
													);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpLegacyHashOpfamily), element_local_name))
	{
		// parse inverse operator id
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_legacy_hash_opfamily = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													attrs,
													EdxltokenMdid,
													EdxltokenGPDBScalarOpLegacyHashOpfamily
													);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBScalarOp::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBScalarOp::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOp), element_local_name))
	{
		// construct the MD scalar operator object from its part
		GPOS_ASSERT(m_mdid->IsValid() && NULL != m_mdname);
		
		GPOS_ASSERT(0 == this->Length() || 1 == this->Length());
		
		IMdIdArray *mdid_opfamilies_array = NULL;
		if (0 < this->Length())
		{
			CParseHandlerMetadataIdList *mdid_list_parse_handler = dynamic_cast<CParseHandlerMetadataIdList*>((*this)[0]);
			mdid_opfamilies_array = mdid_list_parse_handler->GetMdIdArray();
			mdid_opfamilies_array->AddRef();
		}
		else 
		{
			mdid_opfamilies_array = GPOS_NEW(m_mp) IMdIdArray(m_mp);
		}
			m_imd_obj = GPOS_NEW(m_mp) CMDScalarOpGPDB
				(
				m_mp,
				m_mdid,
				m_mdname,
				m_mdid_type_left,
				m_mdid_type_right,
				m_mdid_type_result,
				m_func_mdid,
				m_mdid_commute_opr,
				m_mdid_inverse_opr,
				m_comparision_type,
				m_returns_null_on_null_input,
				mdid_opfamilies_array,
				m_mdid_hash_opfamily,
				m_mdid_legacy_hash_opfamily,
				m_is_ndv_preserving
				)
				;
		
		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();

	}
	else if (!IsSupportedChildElem(element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBScalarOp::FSupportedElem
//
//	@doc:
//		Is this a supported child elem of the scalar op
//
//---------------------------------------------------------------------------
BOOL
CParseHandlerMDGPDBScalarOp::IsSupportedChildElem
	(
	const XMLCh* const xml_str
	)
{
	return (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpLeftTypeId), xml_str) ||
			0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpRightTypeId), xml_str) ||
			0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpResultTypeId), xml_str) ||
			0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpFuncId), xml_str) ||
			0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpCommOpId), xml_str) ||
			0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpInverseOpId), xml_str) ||
			0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpHashOpfamily), xml_str) ||
			0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBScalarOpLegacyHashOpfamily), xml_str));
}

// EOF
