//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CGPDBTypeHelper.h
//
//	@doc:
//		Helper class that provides implementation for common functions across
//		different GPDB types (CMDTypeInt4GPDB, CMDTypeBoolGPDB, and CMDTypeGenericGPDB)
//---------------------------------------------------------------------------
#ifndef GPMD_CGPDBHelper_H
#define GPMD_CGPDBHelper_H

#include "gpos/base.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"


namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

template <class T>
class CGPDBTypeHelper
{
public:
	// serialize object in DXL format
	static void
	Serialize(CXMLSerializer *xml_serializer, const T *mdtype)
	{
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMDType));

		mdtype->MDId()->Serialize(xml_serializer,
								  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));

		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
									 mdtype->Mdname().GetMDName());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeRedistributable),
			mdtype->IsRedistributable());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeHashable),
			mdtype->IsHashable());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeMergeJoinable),
			mdtype->IsMergeJoinable());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeComposite),
			mdtype->IsComposite());

		if (mdtype->IsComposite())
		{
			mdtype->GetBaseRelMdid()->Serialize(
				xml_serializer,
				CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeRelid));
		}

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeIsTextRelated),
			mdtype->IsTextRelated());

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeFixedLength),
			mdtype->IsFixedLength());

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeLength),
			mdtype->GetGPDBLength());

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeByValue),
			mdtype->IsPassedByValue());

		mdtype->SerializeMDIdAsElem(
			xml_serializer,
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeDistrOpfamily),
			mdtype->m_distr_opfamily);
		mdtype->SerializeMDIdAsElem(
			xml_serializer,
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeLegacyDistrOpfamily),
			mdtype->m_legacy_distr_opfamily);
		mdtype->SerializeMDIdAsElem(
			xml_serializer,
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypePartOpfamily),
			mdtype->m_part_opfamily);

		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeEqOp),
			mdtype->GetMdidForCmpType(IMDType::EcmptEq));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeNEqOp),
			mdtype->GetMdidForCmpType(IMDType::EcmptNEq));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeLTOp),
			mdtype->GetMdidForCmpType(IMDType::EcmptL));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeLEqOp),
			mdtype->GetMdidForCmpType(IMDType::EcmptLEq));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeGTOp),
			mdtype->GetMdidForCmpType(IMDType::EcmptG));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeGEqOp),
			mdtype->GetMdidForCmpType(IMDType::EcmptGEq));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeCompOp),
			mdtype->CmpOpMdid());
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeArray),
			mdtype->GetArrayTypeMdid());

		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeAggMin),
			mdtype->GetMdidForAggType(IMDType::EaggMin));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeAggMax),
			mdtype->GetMdidForAggType(IMDType::EaggMax));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeAggAvg),
			mdtype->GetMdidForAggType(IMDType::EaggAvg));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeAggSum),
			mdtype->GetMdidForAggType(IMDType::EaggSum));
		mdtype->SerializeMDIdAsElem(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeAggCount),
			mdtype->GetMdidForAggType(IMDType::EaggCount));

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMDType));

		GPOS_CHECK_ABORT;
	}

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	static void
	DebugPrint(IOstream &os, const T *mdtype)
	{
		os << "Type id: ";
		mdtype->MDId()->OsPrint(os);
		os << std::endl;

		os << "Type name: " << mdtype->Mdname().GetMDName()->GetBuffer()
		   << std::endl;

		const CWStringConst *redistributable_str =
			mdtype->IsRedistributable()
				? CDXLTokens::GetDXLTokenStr(EdxltokenTrue)
				: CDXLTokens::GetDXLTokenStr(EdxltokenFalse);

		os << "Redistributable: " << redistributable_str->GetBuffer()
		   << std::endl;

		const CWStringConst *fixed_len_str =
			mdtype->IsFixedLength()
				? CDXLTokens::GetDXLTokenStr(EdxltokenTrue)
				: CDXLTokens::GetDXLTokenStr(EdxltokenFalse);

		os << "Fixed length: " << fixed_len_str->GetBuffer() << std::endl;

		if (mdtype->IsFixedLength())
		{
			os << "Type length: " << mdtype->Length() << std::endl;
		}

		const CWStringConst *passed_by_val_str =
			mdtype->IsPassedByValue()
				? CDXLTokens::GetDXLTokenStr(EdxltokenTrue)
				: CDXLTokens::GetDXLTokenStr(EdxltokenFalse);

		os << "Pass by value: " << passed_by_val_str->GetBuffer() << std::endl;

		os << "Equality operator id: ";
		mdtype->GetMdidForCmpType(IMDType::EcmptEq)->OsPrint(os);
		os << std::endl;

		os << "Less-than operator id: ";
		mdtype->GetMdidForCmpType(IMDType::EcmptL)->OsPrint(os);
		os << std::endl;

		os << "Greater-than operator id: ";
		mdtype->GetMdidForCmpType(IMDType::EcmptG)->OsPrint(os);
		os << std::endl;

		os << "Comparison operator id: ";
		mdtype->CmpOpMdid()->OsPrint(os);
		os << std::endl;

		os << std::endl;
	}
#endif	// GPOS_DEBUG
};
}  // namespace gpmd

#endif	// !CGPMD_GPDBHelper_H

// EOF
