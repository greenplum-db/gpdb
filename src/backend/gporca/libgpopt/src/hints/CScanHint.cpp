//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CScanHint.cpp
//
//	@doc:
//		Container of plan hint objects
//---------------------------------------------------------------------------

#include "gpopt/hints/CScanHint.h"

#include "gpopt/exception.h"
#include "gpopt/hints/CHintUtils.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpopt;

FORCE_GENERATE_DBGSTR(CScanHint);


BOOL
CScanHint::SatisfiesOperator(COperator *op) const
{
	BOOL is_satisfied = true;

	CEHintTypeSetSetIter type_info(*m_types);
	while (type_info.Advance() && is_satisfied)
	{
		switch (type_info.TBit())
		{
			case SeqScan:
			{
				is_satisfied = op->Eopid() == COperator::EopLogicalDynamicGet ||
							   op->Eopid() == COperator::EopLogicalGet;
				break;
			}
			case NoSeqScan:
			{
				is_satisfied = op->Eopid() != COperator::EopLogicalDynamicGet &&
							   op->Eopid() != COperator::EopLogicalGet;
				break;
			}
			case IndexScan:
			{
				is_satisfied =
					op->Eopid() == COperator::EopLogicalDynamicIndexGet ||
					op->Eopid() == COperator::EopLogicalIndexGet;
				break;
			}
			case NoIndexScan:
			{
				is_satisfied =
					op->Eopid() != COperator::EopLogicalDynamicIndexGet &&
					op->Eopid() != COperator::EopLogicalIndexGet;
				break;
			}
			case IndexOnlyScan:
			{
				is_satisfied =
					op->Eopid() == COperator::EopLogicalDynamicIndexOnlyGet ||
					op->Eopid() == COperator::EopLogicalIndexOnlyGet;
				break;
			}
			case NoIndexOnlyScan:
			{
				is_satisfied =
					op->Eopid() != COperator::EopLogicalDynamicIndexOnlyGet &&
					op->Eopid() != COperator::EopLogicalIndexOnlyGet;
				break;
			}
			case BitmapScan:
			{
				is_satisfied =
					op->Eopid() == COperator::EopScalarBitmapIndexProbe;
				break;
			}
			case NoBitmapScan:
			{
				is_satisfied =
					op->Eopid() != COperator::EopScalarBitmapIndexProbe;
				break;
			}
			default:
			{
				CWStringDynamic *error_message = GPOS_NEW(m_mp)
					CWStringDynamic(m_mp, GPOS_WSZ_LIT("Unknown scan type: "));

				GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
						   error_message->GetBuffer());
			}
		}
	}
	return is_satisfied;
}

IOstream &
CScanHint::OsPrint(IOstream &os) const
{
	os << "ScanHint: " << m_name->GetBuffer() << "[";
	os << "indexes:";
	CWStringDynamic *indexes =
		CDXLUtils::SerializeToCommaSeparatedString(m_mp, GetIndexNames());
	os << indexes->GetBuffer();
	GPOS_DELETE(indexes);

	os << " types:";
	CWStringDynamic *hints =
		CDXLUtils::Serialize(m_mp, m_types, CHintUtils::ScanHintEnumToString);
	os << hints->GetBuffer();
	GPOS_DELETE(hints);

	os << "]";
	return os;
}

void
CScanHint::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenScanHint));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenAlias),
								 GetName());

	CWStringDynamic *hints =
		CDXLUtils::Serialize(m_mp, m_types, CHintUtils::ScanHintEnumToString);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenOpName),
								 hints);
	GPOS_DELETE(hints);

	if (GetIndexNames()->Size() > 0)
	{
		CWStringDynamic *indexes =
			CDXLUtils::SerializeToCommaSeparatedString(m_mp, GetIndexNames());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenIndexName), indexes);
		GPOS_DELETE(indexes);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenScanHint));
}
