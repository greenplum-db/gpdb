//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Greenplum, Inc.
//
//	@filename:
//		CCatalogUtils.cpp
//
//	@doc:
//		Routines to extract interesting information from the catalog
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/utils/CCatalogUtils.h"

//---------------------------------------------------------------------------
//	@function:
//		CCatalogUtils::GetRelationOids
//
//	@doc:
//		Return list of relation oids from the catalog
//
//---------------------------------------------------------------------------
List *
CCatalogUtils::GetRelationOids()
{
	return relation_oids();
}

//---------------------------------------------------------------------------
//	@function:
//		CCatalogUtils::GetOperatorOids
//
//	@doc:
//		Return list of operator oids from the catalog
//
//---------------------------------------------------------------------------
List *
CCatalogUtils::GetOperatorOids()
{
	return operator_oids();
}

//---------------------------------------------------------------------------
//	@function:
//		CCatalogUtils::GetFunctionOids
//
//	@doc:
//		Return list of function oids_list from the catalog
//
//---------------------------------------------------------------------------
List *
CCatalogUtils::GetFunctionOids()
{
	return function_oids();
}

//---------------------------------------------------------------------------
//	@function:
//		CCatalogUtils::GetAllOids
//
//	@doc:
//		Return list of all oids_list from the catalog
//
//---------------------------------------------------------------------------
List *CCatalogUtils::GetAllOids()
{
	return list_concat(list_concat(GetRelationOids(), GetOperatorOids()), GetFunctionOids());
}

// EOF
