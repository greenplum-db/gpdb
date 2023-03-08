//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		IMDIndex.cpp
//
//	@doc:
//		Implementation of MD index
//---------------------------------------------------------------------------

#include "naucrates/md/IMDIndex.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		IMDRelation::GetDistrPolicyStr
//
//	@doc:
//		Return relation distribution policy as a string value
//
//---------------------------------------------------------------------------
const CWStringConst *
IMDIndex::GetDXLStr(EmdindexType index_type)
{
	switch (index_type)
	{
		case EmdindBtree:
			return CDXLTokens::GetDXLTokenStr(EdxltokenIndexTypeBtree);
		case EmdindBitmap:
			return CDXLTokens::GetDXLTokenStr(EdxltokenIndexTypeBitmap);
		case EmdindGist:
			return CDXLTokens::GetDXLTokenStr(EdxltokenIndexTypeGist);
		case EmdindGin:
			return CDXLTokens::GetDXLTokenStr(EdxltokenIndexTypeGin);
		case EmdindBrin:
			return CDXLTokens::GetDXLTokenStr(EdxltokenIndexTypeBrin);
		case EmdindHash:
			return CDXLTokens::GetDXLTokenStr(EdxltokenIndexTypeHash);
		default:
			GPOS_ASSERT(!"Unrecognized index type");
			return nullptr;
	}
}

// EOF
