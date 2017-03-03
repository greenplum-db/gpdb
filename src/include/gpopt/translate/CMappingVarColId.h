//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMappingVarColId.h
//
//	@doc:
//		Abstract base class of VAR to DXL mapping during scalar operator translation.
//		If we need a scalar translator during PlStmt->DXL or Query->DXL translation
//		we implement a variable mapping for PlStmt or Query respectively that
//		is derived from this interface.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CMappingVarColId_H
#define GPDXL_CMappingVarColId_H


#include "gpopt/translate/CGPDBAttInfo.h"
#include "gpopt/translate/CGPDBAttOptCol.h"

#include "gpos/common/CHashMap.h"
#include "gpos/common/CHashMapIter.h"

#include "naucrates/dxl/operators/dxlops.h"
#include "naucrates/dxl/CIdGenerator.h"

//fwd decl
struct Var;
struct List;

namespace gpmd
{
	class IMDIndex;
}

namespace gpdxl
{
	// physical operator types used in planned statements
	enum EPlStmtPhysicalOpType
	{
		EpspotTblScan,
		EpspotHashjoin,
		EpspotNLJoin,
		EpspotMergeJoin,
		EpspotMotion,
		EpspotLimit,
		EpspotAgg,
		EpspotWindow,
		EpspotSort,
		EpspotSubqueryScan,
		EpspotAppend,
		EpspotResult,
		EpspotMaterialize,
		EpspotSharedScan,
		EpspotIndexScan,
		EpspotIndexOnlyScan,
		EpspotNone
	};

	//---------------------------------------------------------------------------
	//	@class:
	//		CMappingVarColId
	//
	//	@doc:
	//		Class providing the interface for VAR to DXL mapping during scalar
	//		operator translation that are used to generate the CDXLNode from a
	//		variable reference in a PlStmt or Query
	//
	//---------------------------------------------------------------------------
	class CMappingVarColId
	{
		private:
			// memory pool
			IMemoryPool *m_pmp;

			// hash map structure to store gpdb att -> opt col information
			typedef CHashMap<CGPDBAttInfo, CGPDBAttOptCol, UlHashGPDBAttInfo, FEqualGPDBAttInfo,
							CleanupRelease, CleanupRelease > CMVCMap;

			// iterator
			typedef CHashMapIter<CGPDBAttInfo, CGPDBAttOptCol, UlHashGPDBAttInfo, FEqualGPDBAttInfo,
							CleanupRelease, CleanupRelease > CMVCMapIter;

			// map from gpdb att to optimizer col
			CMVCMap	*m_pmvcmap;

			// insert mapping entry
			void Insert(ULONG, ULONG, INT, ULONG, CWStringBase *pstr);

			// no copy constructor
			CMappingVarColId(const CMappingVarColId &);

			// helper function to access mapping
			const CGPDBAttOptCol *Pgpdbattoptcol
								(
								ULONG ulCurrentQueryLevel,
								const Var *pvar,
								EPlStmtPhysicalOpType eplsphoptype
								)
								const;

		public:

			// ctor
			explicit
			CMappingVarColId(IMemoryPool *);

			// dtor
			virtual
			~CMappingVarColId()
			{
				m_pmvcmap->Release();
			}

			// given a gpdb attribute, return a column name in optimizer world
			virtual
			const CWStringBase *PstrColName
											(
											ULONG ulCurrentQueryLevel,
											const Var *pvar,
											EPlStmtPhysicalOpType eplsphoptype
											)
											const;

			// given a gpdb attribute, return column id
			virtual
			ULONG UlColId
							(
							ULONG ulCurrentQueryLevel,
							const Var *pvar,
							EPlStmtPhysicalOpType eplsphoptype
							)
							const;

			// load up mapping information from an index
			void LoadIndexColumns(ULONG ulQueryLevel, ULONG ulRTEIndex, const IMDIndex *pmdindex, const CDXLTableDescr *pdxltabdesc);

			// load up mapping information from table descriptor
			void LoadTblColumns(ULONG ulQueryLevel, ULONG ulRTEIndex, const CDXLTableDescr *pdxltabdesc);

			// load up column id mapping information from the array of column descriptors
			void LoadColumns(ULONG ulQueryLevel, ULONG ulRTEIndex, const DrgPdxlcd *pdrgdxlcd);

			// load up mapping information from derived table columns
			void LoadDerivedTblColumns(ULONG ulQueryLevel, ULONG ulRTEIndex, const DrgPdxln *pdrgpdxlnDerivedColumns, List *plTargetList);

			// load information from CTE columns
			void LoadCTEColumns(ULONG ulQueryLevel, ULONG ulRTEIndex, const DrgPul *pdrgpulCTE, List *plTargetList);

			// load up mapping information from scalar projection list
			void LoadProjectElements(ULONG ulQueryLevel, ULONG ulRTEIndex, const CDXLNode *pdxlnPrL);

			// load up mapping information from list of column names
			void Load(ULONG ulQueryLevel, ULONG ulRTEIndex,	CIdGenerator *pidgtor, List *plColNames);

			// create a deep copy
			CMappingVarColId *PmapvarcolidCopy(IMemoryPool *pmp) const;

			// create a deep copy
			CMappingVarColId *PmapvarcolidCopy(ULONG ulQueryLevel) const;
			
			// create a copy of the mapping replacing old col ids with new ones
			CMappingVarColId *PmapvarcolidRemap
				(
				IMemoryPool *pmp,
				DrgPul *pdrgpulOld,
				DrgPul *pdrgpulNew
				)
				const;
	};
}

#endif //GPDXL_CMappingVarColId_H

// EOF
