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
		// return list of relation oids_list in catalog
		static
		List *GetRelationOids();

		// return list of operator oids_list in catalog
		static
		List *GetOperatorOids();

		// return list of function oids_list in catalog
		static
		List *GetFunctionOids();

	public:

		// return list of all object oids_list in catalog
		static
		List *GetAllOids();
};

#endif // CCatalogUtils_H

// EOF
