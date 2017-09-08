//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorRelcacheToDXL.h
//
//	@doc:
//		Class for translating GPDB's relcache entries into DXL MD objects
//
//	@test:
//
//
//---------------------------------------------------------------------------



#ifndef GPDXL_CTranslatorRelcacheToDXL_H
#define GPDXL_CTranslatorRelcacheToDXL_H

#include "gpos/base.h"
#include "c.h"
#include "postgres.h"
#include "access/tupdesc.h"
#include "catalog/gp_policy.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/operators/CDXLColDescr.h"

#include "naucrates/md/IMDId.h"
#include "naucrates/md/CMDRelationGPDB.h"
#include "naucrates/md/CMDRelationExternalGPDB.h"
#include "naucrates/md/CMDAggregateGPDB.h"
#include "naucrates/md/CMDFunctionGPDB.h"
#include "naucrates/md/CMDTriggerGPDB.h"
#include "naucrates/md/CMDCheckConstraintGPDB.h"
#include "naucrates/md/CMDPartConstraintGPDB.h"
#include "naucrates/md/CMDScalarOpGPDB.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDFunction.h"
#include "naucrates/md/CDXLColStats.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/md/IMDIndex.h"

// fwd decl
struct RelationData;
typedef struct RelationData* Relation;
struct LogicalIndexes;
struct LogicalIndexInfo;

namespace gpdxl
{
	using namespace gpos;
	using namespace gpmd;

	//---------------------------------------------------------------------------
	//	@class:
	//		CTranslatorRelcacheToDXL
	//
	//	@doc:
	//		Class for translating GPDB's relcache entries into DXL MD objects
	//
	//---------------------------------------------------------------------------
	class CTranslatorRelcacheToDXL
	{
		private:

		//---------------------------------------------------------------------------
		//	@class:
		//		SFuncProps
		//
		//	@doc:
		//		Internal structure to capture exceptional cases where
		//		function properties are wrongly defined in the catalog,
		//
		//		this information is used to correct function properties during
		//		translation
		//
		//---------------------------------------------------------------------------
		struct SFuncProps
		{

			private:

				// function identifier
				OID m_oid;

				// function stability
				IMDFunction::EFuncStbl m_efs;

				// function data access
				IMDFunction::EFuncDataAcc m_efda;

				// is function strict?
				BOOL m_fStrict;

				// can the function return multiple rows?
				BOOL m_fReturnsSet;

			public:

				// ctor
				SFuncProps
					(
					OID oid,
					IMDFunction::EFuncStbl efs,
					IMDFunction::EFuncDataAcc efda,
					BOOL fStrict,
					BOOL fReturnsSet
					)
					:
					m_oid(oid),
					m_efs(efs),
					m_efda(efda),
					m_fStrict(fStrict),
					m_fReturnsSet(fReturnsSet)
				{}

				// dtor
				virtual
				~SFuncProps()
				{};

				// return function identifier
				OID Oid() const
				{
					return m_oid;
				}

				// return function stability
				IMDFunction::EFuncStbl Efs() const
				{
					return m_efs;
				}

				// return data access property
				IMDFunction::EFuncDataAcc Efda() const
				{
					return m_efda;
				}

				// is function strict?
				BOOL FStrict() const
				{
					return m_fStrict;
				}

				// does function return set?
				BOOL FReturnsSet() const
				{
					return m_fReturnsSet;
				}

			}; // struct SFuncProps

			// array of function properties map
			static
			const SFuncProps m_rgfp[];

			// lookup function properties
			static
			void LookupFuncProps
				(
				OID oidFunc,
				IMDFunction::EFuncStbl *pefs, // output: function stability
				IMDFunction::EFuncDataAcc *pefda, // output: function data access
				BOOL *fStrict, // output: is function strict?
				BOOL *fReturnsSet // output: does function return set?
				);

			// check and fall back for unsupported relations
			static
			void CheckUnsupportedRelation(OID oidRel);

			// get type name from the relcache
			static
			CMDName *PmdnameType(IMemoryPool *pmp, IMDId *pmdid);

			// get function stability property from the GPDB character representation
			static
			CMDFunctionGPDB::EFuncStbl EFuncStability(CHAR c);

			// get function data access property from the GPDB character representation
			static
			CMDFunctionGPDB::EFuncDataAcc EFuncDataAccess(CHAR c);

			// get type of aggregate's intermediate result from the relcache
			static
			IMDId *PmdidAggIntermediateResultType(IMemoryPool *pmp, IMDId *pmdid);

			// retrieve a GPDB metadata object from the relcache
			static
			IMDCacheObject *PimdobjGPDB(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdid);

			// retrieve relstats object from the relcache
			static
			IMDCacheObject *PimdobjRelStats(IMemoryPool *pmp, IMDId *pmdid);

			// retrieve column stats object from the relcache
			static
			IMDCacheObject *PimdobjColStats(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdid);

			// retrieve cast object from the relcache
			static
			IMDCacheObject *PimdobjCast(IMemoryPool *pmp, IMDId *pmdid);
			
			// retrieve scalar comparison object from the relcache
			static
			IMDCacheObject *PmdobjScCmp(IMemoryPool *pmp, IMDId *pmdid);

			// transform GPDB's MCV information to optimizer's histogram structure
			static
			CHistogram *PhistTransformGPDBMCV
								(
								IMemoryPool *pmp,
								const IMDType *pmdtype,
								const Datum *pdrgdatumMCVValues,
								const float4 *pdrgfMCVFrequencies,
								ULONG ulNumMCVValues
								);

			// transform GPDB's hist information to optimizer's histogram structure
			static
			CHistogram *PhistTransformGPDBHist
								(
								IMemoryPool *pmp,
								const IMDType *pmdtype,
								const Datum *pdrgdatumHistValues,
								ULONG ulNumHistValues,
								CDouble dDistinctHist,
								CDouble dFreqHist
								);

			// histogram to array of dxl buckets
			static
			DrgPdxlbucket *Pdrgpdxlbucket
								(
								IMemoryPool *pmp,
								const IMDType *pmdtype,
								const CHistogram *phist
								);

			// transform stats from pg_stats form to optimizer's preferred form
			static
			DrgPdxlbucket *PdrgpdxlbucketTransformStats
								(
								IMemoryPool *pmp,
								OID oidAttType,
								CDouble dDistinct,
								CDouble dNullFreq,
								const Datum *pdrgdatumMCVValues,
								const float4 *pdrgfMCVFrequencies,
								ULONG ulNumMCVValues,
								const Datum *pdrgdatumHistValues,
								ULONG ulNumHistValues
								);

			// get partition keys and types for a relation
			static
			void GetPartKeysAndTypes(IMemoryPool *pmp, Relation rel, OID oid, DrgPul **pdrgpulPartKeys, DrgPsz **pdrgpszPartTypes);

			// get keysets for relation
			static
			DrgPdrgPul *PdrgpdrgpulKeys(IMemoryPool *pmp, OID oid, BOOL fAddDefaultKeys, BOOL fPartitioned, ULONG *pulMapping);

			// storage type for a relation
			static
			IMDRelation::Erelstoragetype Erelstorage(CHAR cStorageType);

			// fix frequencies if they add up to more than 1.0
			static
			void NormalizeFrequencies(float4 *pdrgf, ULONG ulLength, CDouble *pdNullFrequency);

			// get the relation columns
			static
			DrgPmdcol *Pdrgpmdcol(IMemoryPool *pmp, CMDAccessor *pmda, Relation rel, IMDRelation::Erelstoragetype erelstorage);

			// return the dxl representation of the column's default value
			static
			CDXLNode *PdxlnDefaultColumnValue(IMemoryPool *pmp, CMDAccessor *pmda, TupleDesc rd_att, AttrNumber attrno);


			// get the distribution columns
			static
			DrgPul *PdrpulDistrCols(IMemoryPool *pmp, GpPolicy *pgppolicy, DrgPmdcol *pdrgpmdcol, ULONG ulSize);

			// construct a mapping GPDB attnos -> position in the column array
			static
			ULONG *PulAttnoMapping(IMemoryPool *pmp, DrgPmdcol *pdrgpmdcol, ULONG ulMaxCols);

			// check if index is supported
			static
			BOOL FIndexSupported(Relation relIndex);
			
			// retrieve index info list of partitioned table
			static
			List *PlIndexInfoPartTable(Relation rel);
			 
			// compute the array of included columns
			static
			DrgPul *PdrgpulIndexIncludedColumns(IMemoryPool *pmp, const IMDRelation *pmdrel);
			
			// is given level included in the default partitions
			static 
			BOOL FDefaultPartition(List *plDefaultLevels, ULONG ulLevel);
			
			// retrieve part constraint for index
			static
			CMDPartConstraintGPDB *PmdpartcnstrIndex
				(
				IMemoryPool *pmp, 
				CMDAccessor *pmda, 
				const IMDRelation *pmdrel, 
				Node *pnodePartCnstr,
				DrgPul *pdrgpulDefaultParts,
				BOOL fUnbounded
				);

			// retrieve part constraint for relation
			static
			CMDPartConstraintGPDB *PmdpartcnstrRelation(IMemoryPool *pmp, CMDAccessor *pmda, OID oidRel, DrgPmdcol *pdrgpmdcol, BOOL fhasIndex);

			// retrieve part constraint from a GPDB node
			static
			CMDPartConstraintGPDB *PmdpartcnstrFromNode
				(
				IMemoryPool *pmp, 
				CMDAccessor *pmda, 
				DrgPdxlcd *pdrgpdxlcd, 
				Node *pnodePartCnstr, 
				DrgPul *pdrgpulDefaultParts,
				BOOL fUnbounded
				);
	
			// return relation name
			static
			CMDName *PmdnameRel(IMemoryPool *pmp, Relation rel);

			// return the index info list defined on the given relation
			static
			DrgPmdIndexInfo *PdrgpmdRelIndexInfo(IMemoryPool *pmp, Relation rel);

			// return index info list of indexes defined on a partitoned table
			static
			DrgPmdIndexInfo *PdrgpmdRelIndexInfoPartTable(IMemoryPool *pmp, Relation relRoot);

			// return index info list of indexes defined on regular, external tables or leaf partitions
			static
			DrgPmdIndexInfo *PdrgpmdRelIndexInfoNonPartTable(IMemoryPool *pmp, Relation rel);

			// retrieve an index over a partitioned table from the relcache
			static
			IMDIndex *PmdindexPartTable(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdidIndex, const IMDRelation *pmdrel, LogicalIndexes *plind);
			
			// lookup an index given its id from the logical indexes structure
			static
			LogicalIndexInfo *PidxinfoLookup(LogicalIndexes *plind, OID oid);
			
			// construct an MD cache index object given its logical index representation
			static
			IMDIndex *PmdindexPartTable(IMemoryPool *pmp, CMDAccessor *pmda, LogicalIndexInfo *pidxinfo, IMDId *pmdidIndex, const IMDRelation *pmdrel);

			// return the triggers defined on the given relation
			static
			DrgPmdid *PdrgpmdidTriggers(IMemoryPool *pmp, Relation rel);

			// return the check constraints defined on the relation with the given oid
			static
			DrgPmdid *PdrgpmdidCheckConstraints(IMemoryPool *pmp, OID oid);

			// does attribute number correspond to a transaction visibility attribute
			static 
			BOOL FTransactionVisibilityAttribute(INT iAttNo);
			
			// does relation type have system columns
			static
			BOOL FHasSystemColumns(char	cRelKind);
			
			// translate Optimizer comparison types to GPDB
			static
			ULONG UlCmpt(IMDType::ECmpType ecmpt);
			
			// retrieve the opfamilies mdids for the given scalar op
			static
			DrgPmdid *PdrgpmdidScOpOpFamilies(IMemoryPool *pmp, IMDId *pmdidScOp);
			
			// retrieve the opfamilies mdids for the given index
			static
			DrgPmdid *PdrgpmdidIndexOpFamilies(IMemoryPool *pmp, IMDId *pmdidIndex);

            // for non-leaf partition tables return the number of child partitions
            // else return 1
            static
            ULONG UlTableCount(OID oidRelation);

            // generate statistics for the system level columns
            static
            CDXLColStats *PdxlcolstatsSystemColumn
                              (
                              IMemoryPool *pmp,
                              OID oidRelation,
                              CMDIdColStats *pmdidColStats,
                              CMDName *pmdnameCol,
                              OID oidAttType,
                              AttrNumber attrnum,
                              DrgPdxlbucket *pdrgpdxlbucket,
                              CDouble dRows
                              );
		public:
			// retrieve a metadata object from the relcache
			static
			IMDCacheObject *Pimdobj(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdid);

			// retrieve a relation from the relcache
			static
			IMDRelation *Pmdrel(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdid);

			// add system columns (oid, tid, xmin, etc) in table descriptors
			static
			void AddSystemColumns(IMemoryPool *pmp, DrgPmdcol *pdrgpmdcol, Relation rel, BOOL fAOTable);

			// retrieve an index from the relcache
			static
			IMDIndex *Pmdindex(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdidIndex);

			// retrieve a check constraint from the relcache
			static
			CMDCheckConstraintGPDB *Pmdcheckconstraint(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdid);

			// populate the attribute number to position mapping
			static
			ULONG *PulAttnoPositionMap(IMemoryPool *pmp, const IMDRelation *pmdrel, ULONG ulRgSize);

			// return the position of a given attribute number
			static
			ULONG UlPosition(INT iAttno, ULONG *pul);

			// retrieve a type from the relcache
			static
			IMDType *Pmdtype(IMemoryPool *pmp, IMDId *pmdid);

			// retrieve a scalar operator from the relcache
			static
			CMDScalarOpGPDB *Pmdscop(IMemoryPool *pmp, IMDId *pmdid);

			// retrieve a function from the relcache
			static
			CMDFunctionGPDB *Pmdfunc(IMemoryPool *pmp, IMDId *pmdid);

			// retrieve an aggregate from the relcache
			static
			CMDAggregateGPDB *Pmdagg(IMemoryPool *pmp, IMDId *pmdid);

			// retrieve a trigger from the relcache
			static
			CMDTriggerGPDB *Pmdtrigger(IMemoryPool *pmp, IMDId *pmdid);
			
			// translate GPDB comparison type
			static
			IMDType::ECmpType Ecmpt(ULONG ulCmpt);
			
			// get the distribution policy of the relation
			static
			IMDRelation::Ereldistrpolicy Ereldistribution(GpPolicy *pgppolicy);
	};
}



#endif // !GPDXL_CTranslatorRelcacheToDXL_H

// EOF
