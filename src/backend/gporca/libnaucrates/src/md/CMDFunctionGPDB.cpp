//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDFunctionGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB-specific functions
//		in the MD cache
//---------------------------------------------------------------------------


#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDFunctionGPDB.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpmd;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::CMDFunctionGPDB
//
//	@doc:
//		Constructs a metadata func
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::CMDFunctionGPDB
	(
	CMemoryPool *mp,
	IMDId *mdid,
	CMDName *mdname,
	IMDId *result_type_mdid,
	IMdIdArray *mdid_array,
	BOOL ReturnsSet,
	EFuncStbl func_stability,
	EFuncDataAcc func_data_access,
	BOOL is_strict,
	BOOL is_ndv_preserving,
	BOOL is_allowed_for_PS
	)
	:
	m_mp(mp),
	m_mdid(mdid),
	m_mdname(mdname),
	m_mdid_type_result(result_type_mdid),
	m_mdid_types_array(mdid_array),
	m_returns_set(ReturnsSet),
	m_func_stability(func_stability),
	m_func_data_access(func_data_access),
	m_is_strict(is_strict),
	m_is_ndv_preserving(is_ndv_preserving),
	m_is_allowed_for_PS(is_allowed_for_PS)
{
	GPOS_ASSERT(m_mdid->IsValid());
	GPOS_ASSERT(EfsSentinel > func_stability);
	GPOS_ASSERT(EfdaSentinel > func_data_access);

	InitDXLTokenArrays();
	m_dxl_str = CDXLUtils::SerializeMDObj(m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::~CMDFunctionGPDB
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::~CMDFunctionGPDB()
{
	m_mdid->Release();
	m_mdid_type_result->Release();
	CRefCount::SafeRelease(m_mdid_types_array);
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::InitDXLTokenArrays
//
//	@doc:
//		Initialize DXL token arrays
//
//---------------------------------------------------------------------------
void
CMDFunctionGPDB::InitDXLTokenArrays()
{
	// stability
	m_dxl_func_stability_array[EfsImmutable] = EdxltokenGPDBFuncImmutable;
	m_dxl_func_stability_array[EfsStable] = EdxltokenGPDBFuncStable;
	m_dxl_func_stability_array[EfsVolatile] = EdxltokenGPDBFuncVolatile;

	// data access
	m_dxl_data_access_array[EfdaNoSQL] = EdxltokenGPDBFuncNoSQL;
	m_dxl_data_access_array[EfdaContainsSQL] = EdxltokenGPDBFuncContainsSQL;
	m_dxl_data_access_array[EfdaReadsSQLData] = EdxltokenGPDBFuncReadsSQLData;
	m_dxl_data_access_array[EfdaModifiesSQLData] = EdxltokenGPDBFuncModifiesSQLData;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::MDId
//
//	@doc:
//		Func id
//
//---------------------------------------------------------------------------
IMDId *
CMDFunctionGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::Mdname
//
//	@doc:
//		Func name
//
//---------------------------------------------------------------------------
CMDName
CMDFunctionGPDB::Mdname() const
{
	return *m_mdname;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::GetResultTypeMdid
//
//	@doc:
//		Type id of result
//
//---------------------------------------------------------------------------
IMDId *
CMDFunctionGPDB::GetResultTypeMdid() const
{
	return m_mdid_type_result;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::OutputArgTypesMdidArray
//
//	@doc:
//		Output argument types
//
//---------------------------------------------------------------------------
IMdIdArray *
CMDFunctionGPDB::OutputArgTypesMdidArray() const
{
	return m_mdid_types_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::ReturnsSet
//
//	@doc:
//		Returns whether function result is a set
//
//---------------------------------------------------------------------------
BOOL
CMDFunctionGPDB::ReturnsSet() const
{
	return m_returns_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::GetOutputArgTypeArrayStr
//
//	@doc:
//		Serialize the array of output argument types into a comma-separated string
//
//---------------------------------------------------------------------------
CWStringDynamic *
CMDFunctionGPDB::GetOutputArgTypeArrayStr() const
{
	GPOS_ASSERT(NULL != m_mdid_types_array);
	CWStringDynamic *str = GPOS_NEW(m_mp) CWStringDynamic(m_mp);

	const ULONG len = m_mdid_types_array->Size();
	for (ULONG ul = 0; ul < len; ul++)
	{
		IMDId *mdid = (*m_mdid_types_array)[ul];
		if (ul == len - 1)
		{
			// last element: do not print a comma
			str->AppendFormat(GPOS_WSZ_LIT("%ls"), mdid->GetBuffer());
		}
		else
		{
			str->AppendFormat(GPOS_WSZ_LIT("%ls%ls"), mdid->GetBuffer(), CDXLTokens::GetDXLTokenStr(EdxltokenComma)->GetBuffer());
		}
	}

	return str;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::Serialize
//
//	@doc:
//		Serialize function metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDFunctionGPDB::Serialize
	(
	CXMLSerializer *xml_serializer
	) 
	const
{
	xml_serializer->OpenElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), 
						CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFunc));
	
	m_mdid->Serialize(xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMdid));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName), m_mdname->GetMDName());
	
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncReturnsSet), m_returns_set);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncStability), GetFuncStabilityStr());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncDataAccess), GetFuncDataAccessStr());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncStrict), m_is_strict);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncNDVPreserving), m_is_ndv_preserving);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncIsAllowedForPS), m_is_allowed_for_PS);

	SerializeMDIdAsElem(xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncResultTypeId), m_mdid_type_result);

	if (NULL != m_mdid_types_array)
	{
		xml_serializer->OpenElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
							CDXLTokens::GetDXLTokenStr(EdxltokenOutputCols));

		CWStringDynamic *output_arg_type_array_str = GetOutputArgTypeArrayStr();
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenTypeIds), output_arg_type_array_str);
		GPOS_DELETE(output_arg_type_array_str);

		xml_serializer->CloseElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
							CDXLTokens::GetDXLTokenStr(EdxltokenOutputCols));

	}
	xml_serializer->CloseElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), 
						CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFunc));
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::GetFuncStabilityStr
//
//	@doc:
//		String representation of function stability
//
//---------------------------------------------------------------------------
const CWStringConst *
CMDFunctionGPDB::GetFuncStabilityStr() const
{
	if (EfsSentinel > m_func_stability)
	{
		return CDXLTokens::GetDXLTokenStr(m_dxl_func_stability_array[m_func_stability]);
	}

	GPOS_ASSERT(!"Unrecognized function stability setting");
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::GetFuncDataAccessStr
//
//	@doc:
//		String representation of function data access
//
//---------------------------------------------------------------------------
const CWStringConst *
CMDFunctionGPDB::GetFuncDataAccessStr() const
{
	if (EfdaSentinel > m_func_data_access)
	{
		return CDXLTokens::GetDXLTokenStr(m_dxl_data_access_array[m_func_data_access]);
	}

	GPOS_ASSERT(!"Unrecognized function data access setting");
	return NULL;
}

#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMDFunctionGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDFunctionGPDB::DebugPrint
	(
	IOstream &os
	)
	const
{
	os << "Function id: ";
	MDId()->OsPrint(os);
	os << std::endl;
	
	os << "Function name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;
	
	os << "Result type id: ";
	GetResultTypeMdid()->OsPrint(os);
	os << std::endl;
	
	const CWStringConst *return_set_str = ReturnsSet() ?
			CDXLTokens::GetDXLTokenStr(EdxltokenTrue): 
			CDXLTokens::GetDXLTokenStr(EdxltokenFalse); 

	os << "Returns set: " << return_set_str->GetBuffer() << std::endl;

	os << "Function is " << GetFuncStabilityStr()->GetBuffer() << std::endl;
	
	os << "Data access: " << GetFuncDataAccessStr()->GetBuffer() << std::endl;

	const CWStringConst *is_strict = IsStrict() ?
			CDXLTokens::GetDXLTokenStr(EdxltokenTrue): 
			CDXLTokens::GetDXLTokenStr(EdxltokenFalse); 

	os << "Is strict: " << is_strict->GetBuffer() << std::endl;
	
	os << std::endl;	
}

#endif // GPOS_DEBUG

// EOF
