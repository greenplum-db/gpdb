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
			ULONG m_ulColId;

			// the query level
			ULONG m_ulQueryLevel;

			// the target entry
			TargetEntry *m_pte;

		public:

			// ctors and dtor
			CMappingElementColIdTE(ULONG, ULONG, TargetEntry *);

			// return the ColId
			ULONG UlColId() const
			{
				return m_ulColId;
			}

			// return the query level
			ULONG UlQueryLevel() const
			{
				return m_ulQueryLevel;
			}

			// return the column name for the given attribute no
			const TargetEntry *Pte() const
			{
				return m_pte;
			}
	};
}

#endif // GPDXL_CMappingElementColIdTE_H

// EOF
