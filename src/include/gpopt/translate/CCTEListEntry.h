//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CCTEListEntry.h
//
//	@doc:
//		Class representing the list of common table expression defined at a
//		query level
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CCTEListEntry_H
#define GPDXL_CCTEListEntry_H

#include "gpos/base.h"
#include "gpos/common/CHashMap.h"

#include "naucrates/dxl/operators/CDXLNode.h"

// fwd declaration
struct Query;
struct List;
struct RangeTblEntry;
struct CommonTableExpr;


using namespace gpos;

namespace gpdxl
{
	
	// hash on character arrays
	inline
	ULONG UlHashSz
		(
		const CHAR *sz
		)
	{
		return gpos::UlHashByteArray((BYTE *) sz, clib::UlStrLen(sz));
	}
	
	// equality on character arrays
	inline
	BOOL FEqualSz(const CHAR *szA, const CHAR *szB)
	{
		return (0 == clib::IStrCmp(szA, szB));
	}
	

	//---------------------------------------------------------------------------
	//	@class:
	//		CCTEListEntry
	//
	//	@doc:
	//		Class representing the list of common table expression defined at a
	//		query level
	//
	//---------------------------------------------------------------------------
	class CCTEListEntry : public CRefCount
	{
		private:

			// pair of DXL CTE producer and target list of the original CTE query
			struct SCTEProducerInfo
			{
				const CDXLNode *m_pdxlnCTEProducer;
				List *m_plTargetList;
				
				// ctor
				SCTEProducerInfo
					(
					const CDXLNode *pdxlnCTEProducer,
					List *plTargetList
					)
					:
					m_pdxlnCTEProducer(pdxlnCTEProducer),
					m_plTargetList(plTargetList)
				{}
			};
			
			// hash maps mapping CHAR *->SCTEProducerInfo
			typedef CHashMap<CHAR, SCTEProducerInfo, UlHashSz, FEqualSz, CleanupNULL, CleanupDelete > HMSzCTEInfo;

			// query level where the CTEs are defined
			ULONG m_ulQueryLevel;

			// CTE producers at that level indexed by their name
			HMSzCTEInfo *m_phmszcteinfo; 

		public:
			// ctor: single CTE 
			CCTEListEntry(IMemoryPool *pmp, ULONG ulQueryLevel, CommonTableExpr *pcte, CDXLNode *pdxlnCTEProducer);
			
			// ctor: multiple CTEs
			CCTEListEntry(IMemoryPool *pmp, ULONG ulQueryLevel, List *plCTE, DrgPdxln *pdrgpdxln);

			// dtor
			virtual
			~CCTEListEntry()
			{
				m_phmszcteinfo->Release();
			};

			// the query level
			ULONG UlQueryLevel() const
			{
				return m_ulQueryLevel;
			}

			// lookup CTE producer by its name
			const CDXLNode *PdxlnCTEProducer(const CHAR *szCTE) const;

			// lookup CTE producer target list by its name
			List *PlCTEProducerTL(const CHAR *szCTE) const;

			// add a new CTE producer for this level
			void AddCTEProducer(IMemoryPool *pmp, CommonTableExpr *pcte, const CDXLNode *pdxlnCTEProducer);
	};

	// hash maps mapping ULONG -> CCTEListEntry
	typedef CHashMap<ULONG, CCTEListEntry, gpos::UlHash<ULONG>, gpos::FEqual<ULONG>,
	CleanupDelete<ULONG>, CleanupRelease > HMUlCTEListEntry;

	// iterator
	typedef CHashMapIter<ULONG, CCTEListEntry, gpos::UlHash<ULONG>, gpos::FEqual<ULONG>,
	CleanupDelete<ULONG>, CleanupRelease > HMIterUlCTEListEntry;

	}
#endif // !GPDXL_CCTEListEntry_H

//EOF
