//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CStateDXLToQuery.h
//
//	@doc:
//		Class providing access to list of output column target entries and
//		the corresponding list of column names.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CStateDXLToQuery_H
#define GPDXL_CStateDXLToQuery_H

#include "gpos/base.h"
#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/operators/CDXLNode.h"

// fwd decl
struct TargetEntry;
struct List;

namespace gpdxl
{

	//---------------------------------------------------------------------------
	//	@class:
	//		CStateDXLToQuery
	//
	//	@doc:
	//		Class providing access to list of output column TargetEntry and
	//		the corresponding list of column names.
	//
	//---------------------------------------------------------------------------
	class CStateDXLToQuery
	{
		private:
			// memory pool
			IMemoryPool *m_pmp;

			// list of target list entries for the output columns
			List *m_plTEColumns;

			// list of output column names
			List *m_plColumnNames;

			// list of column ids
			DrgPul *m_pdrgpulColIds;

		public:

			CStateDXLToQuery(IMemoryPool *pmp);

			~CStateDXLToQuery();

			void AddOutputColumnEntry(TargetEntry *pte, CHAR *szColname, ULONG ul);

			// return the target entry at the position ulIndex
			TargetEntry *PteColumn(ULONG ulIndex);

			// target entry found in the state
			BOOL FTEFound(TargetEntry *pte);

			// return the column at the position ulIndex
			CHAR* SzColumnName(ULONG ulIndex);

			// return the column id at the position ulIndex
			ULONG UlColId(ULONG ulIndex);

			// number of entries in the state
			ULONG UlLength();

			// clear all entries
			void Clear();

			// clear all entries and load content from another state
			void Reload(CStateDXLToQuery *pstatedxltoquery);
	};
}
#endif // GPDXL_CStateDXLToQuery_H

// EOF
