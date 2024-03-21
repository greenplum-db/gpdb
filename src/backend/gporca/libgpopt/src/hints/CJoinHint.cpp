//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2024 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CJoinHint.cpp
//
//	@doc:
//		Container of join hint objects
//---------------------------------------------------------------------------

#include "gpopt/hints/CJoinHint.h"

#include "gpopt/exception.h"
#include "gpopt/hints/CHintUtils.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpopt;

FORCE_GENERATE_DBGSTR(CJoinHint);
FORCE_GENERATE_DBGSTR(CJoinHint::JoinNode);

//---------------------------------------------------------------------------
//	@function:
//		SerializeJoinOrderHint
//
//	@doc:
//		Serialize a JoinNode into the original Leading hint string. Return the
//		serialized string.
//---------------------------------------------------------------------------
static CWStringDynamic *
SerializeJoinOrderHint(CMemoryPool *mp, const CJoinHint::JoinNode *join_pair)
{
	CWStringDynamic *result = GPOS_NEW(mp) CWStringDynamic(mp);

	if (nullptr != join_pair->GetName())
	{
		result->AppendFormat(GPOS_WSZ_LIT("%ls"),
							 join_pair->GetName()->GetBuffer());
	}
	else
	{
		if (join_pair->IsDirected())
		{
			result->AppendFormat(GPOS_WSZ_LIT("%ls"), GPOS_WSZ_LIT("("));
		}

		CWStringDynamic *str_outer =
			SerializeJoinOrderHint(mp, join_pair->GetOuter());
		result->AppendFormat(GPOS_WSZ_LIT("%ls"), str_outer->GetBuffer());
		GPOS_DELETE(str_outer);

		result->AppendFormat(GPOS_WSZ_LIT("%ls"), GPOS_WSZ_LIT(" "));

		CWStringDynamic *str_inner =
			SerializeJoinOrderHint(mp, join_pair->GetInner());
		result->AppendFormat(GPOS_WSZ_LIT("%ls"), str_inner->GetBuffer());
		GPOS_DELETE(str_inner);

		if (join_pair->IsDirected())
		{
			result->AppendFormat(GPOS_WSZ_LIT("%ls"), GPOS_WSZ_LIT(")"));
		}
	}
	return result;
}

IOstream &
CJoinHint::JoinNode::OsPrint(IOstream &os) const
{
	CAutoMemoryPool amp;
	CWStringDynamic *dxl_string = SerializeJoinOrderHint(amp.Pmp(), this);
	os << dxl_string->GetBuffer();

	GPOS_DELETE(dxl_string);

	return os;
}

CJoinHint::CJoinHint(CMemoryPool *mp, JoinNode *join_pair)
	: m_mp(mp),
	  m_join_node(join_pair),
	  m_aliases(GPOS_NEW(m_mp) StringPtrArray(m_mp))
{
}


//---------------------------------------------------------------------------
//	@function:
//		CJoinHint::GetAliasNames
//
//	@doc:
//		Returns a sorted array containing all table (alias) names specified in
//		the hint.
//---------------------------------------------------------------------------
const StringPtrArray *
CJoinHint::GetAliasNames() const
{
	if (m_aliases->Size() == 0)
	{
		CWStringConstHashSet *aliasset =
			CHintUtils::GetAliasesFromHint(m_mp, m_join_node);
		CWStringConstHashSetIter hsiter(aliasset);
		while (hsiter.Advance())
		{
			m_aliases->Append(
				GPOS_NEW(m_mp) CWStringConst(m_mp, hsiter.Get()->GetBuffer()));
		}
		aliasset->Release();

		m_aliases->Sort(CWStringBase::Compare);
	}
	return m_aliases;
}


IOstream &
CJoinHint::OsPrint(IOstream &os) const
{
	os << "JoinHint:";

	m_join_node->OsPrint(os);

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CJoinHint::Serialize
//
//	@doc:
//		Serialize the object
//---------------------------------------------------------------------------
void
CJoinHint::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenJoinHint));

	CWStringDynamic *aliases =
		CDXLUtils::SerializeToCommaSeparatedString(m_mp, GetAliasNames());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenAlias),
								 aliases);
	GPOS_DELETE(aliases);

	CWStringDynamic *dxl_string = SerializeJoinOrderHint(m_mp, GetJoinNode());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenLeading),
								 dxl_string);
	GPOS_DELETE(dxl_string);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenJoinHint));
}
