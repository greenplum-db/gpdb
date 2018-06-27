//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CCatalogUtils.h
//
//	@doc:
//		Routines to extract interesting information from the catalog
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef CCatalogUtils_H
#define CCatalogUtils_H

#include "gpdbdefs.h"


class CCatalogUtils {

	private:
		// return list of relation plOids in catalog
		static
		List *GetRelationOids();

		// return list of operator plOids in catalog
		static
		List *GetOperatorOids();

		// return list of function plOids in catalog
		static
		List *GetFunctionOids();

	public:

		// return list of all object plOids in catalog
		static
		List *GetAllOids();
};

#endif // CCatalogUtils_H

// EOF
