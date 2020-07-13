//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CLogicalDynamicGetBase.h
//
//	@doc:
//		Base class for dynamic table accessors for partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDynamicGetBase_H
#define GPOPT_CLogicalDynamicGetBase_H

#include "gpos/base.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
	
	// fwd declarations
	class CTableDescriptor;
	class CName;
	class CColRefSet;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalDynamicGetBase
	//
	//	@doc:
	//		Dynamic table accessor base class
	//
	//---------------------------------------------------------------------------
	class CLogicalDynamicGetBase : public CLogical
	{

		protected:

			// alias for table
			const CName *m_pnameAlias;

			// table descriptor
			CTableDescriptor *m_ptabdesc;
			
			// dynamic scan id
			ULONG m_scan_id;
			
			// output columns
			CColRefArray *m_pdrgpcrOutput;
			
			// partition keys
			CColRef2dArray *m_pdrgpdrgpcrPart;

			// secondary scan id in case of a partial scan
			ULONG m_ulSecondaryScanId;
			
			// is scan partial -- only used with heterogeneous indexes defined on a subset of partitions
			BOOL m_is_partial;
			
			// dynamic get part constraint
			CPartConstraint *m_part_constraint;
			
			// relation part constraint
			CPartConstraint *m_ppartcnstrRel;
			
			// distribution columns (empty for master only tables)
			CColRefSet *m_pcrsDist;

			// private copy ctor
			CLogicalDynamicGetBase(const CLogicalDynamicGetBase &);

			// given a colrefset from a table, get colids and attno
			void
			ExtractColIdsAttno(CMemoryPool *mp, CColRefSet *pcrs, ULongPtrArray *colids, ULongPtrArray *pdrgpulPos) const;

			// derive stats from base table using filters on partition and/or index columns
			IStatistics *PstatsDeriveFilter(CMemoryPool *mp, CExpressionHandle &exprhdl, CExpression *pexprFilter) const;

		public:
		
			// ctors
			explicit
			CLogicalDynamicGetBase(CMemoryPool *mp);

			CLogicalDynamicGetBase
				(
				CMemoryPool *mp,
				const CName *pnameAlias,
				CTableDescriptor *ptabdesc,
				ULONG scan_id,
				CColRefArray *colref_array,
				CColRef2dArray *pdrgpdrgpcrPart,
				ULONG ulSecondaryScanId,
				BOOL is_partial,
				CPartConstraint *ppartcnstr, 
				CPartConstraint *ppartcnstrRel
				);
			
			CLogicalDynamicGetBase
				(
				CMemoryPool *mp,
				const CName *pnameAlias,
				CTableDescriptor *ptabdesc,
				ULONG scan_id
				);

			// dtor
			virtual 
			~CLogicalDynamicGetBase();

			// accessors
			virtual
			CColRefArray *PdrgpcrOutput() const
			{
				return m_pdrgpcrOutput;
			}
			
			// return table's name
			virtual
			const CName &Name() const
			{
				return *m_pnameAlias;
			}
			
			// distribution columns
			virtual
			const CColRefSet *PcrsDist() const
			{
				return m_pcrsDist;
			}

			// return table's descriptor
			virtual
			CTableDescriptor *Ptabdesc() const
			{
				return m_ptabdesc;
			}
			
			// return scan id
			virtual
			ULONG ScanId() const
			{
				return m_scan_id;
			}
			
			// return the partition columns
			virtual
			CColRef2dArray *PdrgpdrgpcrPart() const
			{
				return m_pdrgpdrgpcrPart;
			}

			// return secondary scan id
			virtual
			ULONG UlSecondaryScanId() const
			{
				return m_ulSecondaryScanId;
			}

			// is this a partial scan -- true if the scan operator corresponds to heterogeneous index
			virtual
			BOOL IsPartial() const
			{
				return m_is_partial;
			}
			
			// return dynamic get part constraint
			virtual
			CPartConstraint *Ppartcnstr() const
			{
				return m_part_constraint;
			}

			// return relation part constraint
			virtual
			CPartConstraint *PpartcnstrRel() const
			{
				return m_ppartcnstrRel;
			}
			
			// set part constraint
			virtual
			void SetPartConstraint(CPartConstraint *ppartcnstr);
			
			// set secondary scan id
			virtual
			void SetSecondaryScanId(ULONG scan_id);
			
			// set scan to partial
			virtual
			void SetPartial();

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive output columns
			virtual
			CColRefSet *DeriveOutputColumns(CMemoryPool *, CExpressionHandle &);

			// derive keys
			virtual 
			CKeyCollection *DeriveKeyCollection(CMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive partition consumer info
			virtual
			CPartInfo *DerivePartitionInfo(CMemoryPool *mp, CExpressionHandle &exprhdl) const;
			
			// derive constraint property
			virtual
			CPropConstraint *DerivePropertyConstraint(CMemoryPool *mp, CExpressionHandle &exprhdl) const;
		
			// derive join depth
			virtual
			ULONG DeriveJoinDepth
				(
				CMemoryPool *, // mp
				CExpressionHandle & // exprhdl
				)
				const
			{
				return 1;
			}

			// derive table descriptor
			virtual
			CTableDescriptor *DeriveTableDescriptor
				(
				CMemoryPool *, // mp
				CExpressionHandle & // exprhdl
				)
				const
			{
				return m_ptabdesc;
			}

	}; // class CLogicalDynamicGetBase

}


#endif // !GPOPT_CLogicalDynamicGetBase_H

// EOF
