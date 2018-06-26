//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMappingElementColIdTE.h
//
//	@doc:
//		Wrapper class providing functions for the mapping element (between ColId and TE)
//		during DXL->Query translation
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CMappingElementColIdTE_H
#define GPDXL_CMappingElementColIdTE_H

#include "postgres.h"

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "nodes/primnodes.h"

// fwd decl
struct TargetEntry;

namespace gpdxl
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CMappingElementColIdTE
	//
	//	@doc:
	//		Wrapper class providing functions for the mapping element (between ColId and TE)
	//		during DXL->Query translation
	//
	//---------------------------------------------------------------------------
	class CMappingElementColIdTE : public CRefCount
	{
		private:

			// the column identifier that is used as the key
			ULONG m_colid;

			// the query level
			ULONG m_query_level;

			// the target entry
			TargetEntry *m_target_entry;

		public:

			// ctors and dtor
			CMappingElementColIdTE(ULONG, ULONG, TargetEntry *);

			// return the ColId
			ULONG GetColId() const
			{
				return m_colid;
			}

			// return the query level
			ULONG GetQueryLevel() const
			{
				return m_query_level;
			}

			// return the column name for the given attribute no
			const TargetEntry *GetTargetEntry() const
			{
				return m_target_entry;
			}
	};
}

#endif // GPDXL_CMappingElementColIdTE_H

// EOF
