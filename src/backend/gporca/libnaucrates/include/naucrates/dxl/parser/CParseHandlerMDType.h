//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerMDType.h
//
//	@doc:
//		SAX parse handler class for GPDB types metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDGPDBType_H
#define GPDXL_CParseHandlerMDGPDBType_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/dxl/xml/dxltokens.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMDType
//
//	@doc:
//		Parse handler for GPDB types metadata
//
//---------------------------------------------------------------------------
class CParseHandlerMDType : public CParseHandlerMetadataObject
{
private:
	// structure mapping DXL mdid names to their corresponding member variable
	struct SMdidMapElem
	{
		// mdid name token
		Edxltoken m_edxltoken;

		// address of the member variable for that name
		IMDId **m_token_mdid;
	};

	// id and version of the type
	IMDId *m_mdid;

	// default distribution (hash) opfamily
	IMDId *m_distr_opfamily;

	// default legacy distribution (hash) opfamily
	IMDId *m_legacy_distr_opfamily;

	// default partition (btree) opfamily
	IMDId *m_part_opfamily;

	// type name
	CMDName *m_mdname;

	// is this a fixed-length type
	BOOL m_istype_fixed_Length;

	// type length
	INT m_type_length;

	// is type redistributable
	BOOL m_is_redistributable;

	// is type passed by value or by reference
	BOOL m_type_passed_by_value;

	// id of equality operator for type
	IMDId *m_mdid_eq_op;

	// id of inequality operator for type
	IMDId *m_mdid_neq_op;

	// id of less than operator for type
	IMDId *m_mdid_lt_op;

	// id of less than equals operator for type
	IMDId *m_mdid_lteq_op;

	// id of greater than operator for type
	IMDId *m_mdid_gt_op;

	// id of greater than equals operator for type
	IMDId *m_mdid_gteq_op;

	// id of comparison operator for type used in btree lookups
	IMDId *m_mdid_cmp_op;

	// id of min aggregate
	IMDId *m_mdid_min_op;

	// id of max aggregate
	IMDId *m_mdid_max_op;

	// id of avg aggregate
	IMDId *m_mdid_avg_op;

	// id of sum aggregate
	IMDId *m_mdid_sum_op;

	// id of count aggregate
	IMDId *m_mdid_count_op;

	// is type hashable
	BOOL m_is_hashable;

	// is type merge joinable on '='
	BOOL m_is_merge_joinable;

	// is type composite
	BOOL m_is_composite;

	// is type text related
	BOOL m_is_text_related;

	// id of the relation corresponding to a composite type
	IMDId *m_mdid_base_rel;

	// id of array type
	IMDId *m_mdid_array_type;

	// handles a SAX start element event
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// handles a SAX endelement event
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

	// parse the value for the given mdid variable name from the attributes
	void ParseMdid(const XMLCh *element_local_name, const Attributes &attrs);

	static BOOL IsBuiltInType(const IMDId *mdid);

public:
	CParseHandlerMDType(const CParseHandlerMDType &) = delete;

	// ctor
	CParseHandlerMDType(CMemoryPool *mp,
						CParseHandlerManager *parse_handler_mgr,
						CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerMDType() override;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDGPDBType_H

// EOF
