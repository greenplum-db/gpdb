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
					BOOL is_strict,
					BOOL ReturnsSet
					)
					:
					m_oid(oid),
					m_efs(efs),
					m_efda(efda),
					m_fStrict(is_strict),
					m_fReturnsSet(ReturnsSet)
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
				BOOL ReturnsSet() const
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
				BOOL *is_strict, // output: is function strict?
				BOOL *ReturnsSet // output: does function return set?
				);

			// check and fall back for unsupported relations
			static
			void CheckUnsupportedRelation(OID oidRel);

			// get type name from the relcache
			static
			CMDName *PmdnameType(IMemoryPool *memory_pool, IMDId *pmdid);

			// get function stability property from the GPDB character representation
			static
			CMDFunctionGPDB::EFuncStbl EFuncStability(CHAR c);

			// get function data access property from the GPDB character representation
			static
			CMDFunctionGPDB::EFuncDataAcc EFuncDataAccess(CHAR c);

			// get type of aggregate's intermediate result from the relcache
			static
			IMDId *PmdidAggIntermediateResultType(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a GPDB metadata object from the relcache
			static
			IMDCacheObject *PimdobjGPDB(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// retrieve relstats object from the relcache
			static
			IMDCacheObject *PimdobjRelStats(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve column stats object from the relcache
			static
			IMDCacheObject *PimdobjColStats(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// retrieve cast object from the relcache
			static
			IMDCacheObject *PimdobjCast(IMemoryPool *memory_pool, IMDId *pmdid);
			
			// retrieve scalar comparison object from the relcache
			static
			IMDCacheObject *PmdobjScCmp(IMemoryPool *memory_pool, IMDId *pmdid);

			// transform GPDB's MCV information to optimizer's histogram structure
			static
			CHistogram *PhistTransformGPDBMCV
								(
								IMemoryPool *memory_pool,
								const IMDType *pmdtype,
								const Datum *pdrgdatumMCVValues,
								const float4 *pdrgfMCVFrequencies,
								ULONG ulNumMCVValues
								);

			// transform GPDB's hist information to optimizer's histogram structure
			static
			CHistogram *PhistTransformGPDBHist
								(
								IMemoryPool *memory_pool,
								const IMDType *pmdtype,
								const Datum *pdrgdatumHistValues,
								ULONG ulNumHistValues,
								CDouble dDistinctHist,
								CDouble dFreqHist
								);

			// histogram to array of dxl buckets
			static
			DXLBucketPtrArray *GetDXLBucketArray
								(
								IMemoryPool *memory_pool,
								const IMDType *pmdtype,
								const CHistogram *phist
								);

			// transform stats from pg_stats form to optimizer's preferred form
			static
			DXLBucketPtrArray *PdrgpdxlbucketTransformStats
								(
								IMemoryPool *memory_pool,
								OID oidAttType,
								CDouble dDistinct,
								CDouble null_freq,
								const Datum *pdrgdatumMCVValues,
								const float4 *pdrgfMCVFrequencies,
								ULONG ulNumMCVValues,
								const Datum *pdrgdatumHistValues,
								ULONG ulNumHistValues
								);

			// get partition keys and types for a relation
			static
			void GetPartKeysAndTypes(IMemoryPool *memory_pool, Relation rel, OID oid, ULongPtrArray **pdrgpulPartKeys, CharPtrArray **pdrgpszPartTypes);

			// get keysets for relation
			static
			ULongPtrArray2D *PdrgpdrgpulKeys(IMemoryPool *memory_pool, OID oid, BOOL fAddDefaultKeys, BOOL fPartitioned, ULONG *pulMapping);

			// storage type for a relation
			static
			IMDRelation::Erelstoragetype GetRelStorageType(CHAR cStorageType);

			// fix frequencies if they add up to more than 1.0
			static
			void NormalizeFrequencies(float4 *pdrgf, ULONG length, CDouble *pdNullFrequency);

			// get the relation columns
			static
			MDColumnPtrArray *Pdrgpmdcol(IMemoryPool *memory_pool, CMDAccessor *md_accessor, Relation rel, IMDRelation::Erelstoragetype rel_storage_type);

			// return the dxl representation of the column's default value
			static
			CDXLNode *PdxlnDefaultColumnValue(IMemoryPool *memory_pool, CMDAccessor *md_accessor, TupleDesc rd_att, AttrNumber attrno);


			// get the distribution columns
			static
			ULongPtrArray *PdrpulDistrCols(IMemoryPool *memory_pool, GpPolicy *pgppolicy, MDColumnPtrArray *mdcol_array, ULONG size);

			// construct a mapping GPDB attnos -> position in the column array
			static
			ULONG *PulAttnoMapping(IMemoryPool *memory_pool, MDColumnPtrArray *mdcol_array, ULONG ulMaxCols);

			// check if index is supported
			static
			BOOL FIndexSupported(Relation relIndex);
			
			// retrieve index info list of partitioned table
			static
			List *PlIndexInfoPartTable(Relation rel);
			 
			// compute the array of included columns
			static
			ULongPtrArray *PdrgpulIndexIncludedColumns(IMemoryPool *memory_pool, const IMDRelation *pmdrel);
			
			// is given level included in the default partitions
			static 
			BOOL FDefaultPartition(List *plDefaultLevels, ULONG ulLevel);
			
			// retrieve part constraint for index
			static
			CMDPartConstraintGPDB *PmdpartcnstrIndex
				(
				IMemoryPool *memory_pool, 
				CMDAccessor *md_accessor, 
				const IMDRelation *pmdrel, 
				Node *pnodePartCnstr,
				ULongPtrArray *level_with_default_part_array,
				BOOL is_unbounded
				);

			// retrieve part constraint for relation
			static
			CMDPartConstraintGPDB *PmdpartcnstrRelation(IMemoryPool *memory_pool, CMDAccessor *md_accessor, OID oidRel, MDColumnPtrArray *mdcol_array, BOOL fhasIndex);

			// retrieve part constraint from a GPDB node
			static
			CMDPartConstraintGPDB *PmdpartcnstrFromNode
				(
				IMemoryPool *memory_pool, 
				CMDAccessor *md_accessor, 
				ColumnDescrDXLArray *pdrgpdxlcd, 
				Node *pnodePartCnstr, 
				ULongPtrArray *level_with_default_part_array,
				BOOL is_unbounded
				);
	
			// return relation name
			static
			CMDName *PmdnameRel(IMemoryPool *memory_pool, Relation rel);

			// return the index info list defined on the given relation
			static
			MDIndexInfoPtrArray *PdrgpmdRelIndexInfo(IMemoryPool *memory_pool, Relation rel);

			// return index info list of indexes defined on a partitoned table
			static
			MDIndexInfoPtrArray *PdrgpmdRelIndexInfoPartTable(IMemoryPool *memory_pool, Relation relRoot);

			// return index info list of indexes defined on regular, external tables or leaf partitions
			static
			MDIndexInfoPtrArray *PdrgpmdRelIndexInfoNonPartTable(IMemoryPool *memory_pool, Relation rel);

			// retrieve an index over a partitioned table from the relcache
			static
			IMDIndex *PmdindexPartTable(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdidIndex, const IMDRelation *pmdrel, LogicalIndexes *plind);
			
			// lookup an index given its id from the logical indexes structure
			static
			LogicalIndexInfo *PidxinfoLookup(LogicalIndexes *plind, OID oid);
			
			// construct an MD cache index object given its logical index representation
			static
			IMDIndex *PmdindexPartTable(IMemoryPool *memory_pool, CMDAccessor *md_accessor, LogicalIndexInfo *pidxinfo, IMDId *pmdidIndex, const IMDRelation *pmdrel);

			// return the triggers defined on the given relation
			static
			MdidPtrArray *PdrgpmdidTriggers(IMemoryPool *memory_pool, Relation rel);

			// return the check constraints defined on the relation with the given oid
			static
			MdidPtrArray *PdrgpmdidCheckConstraints(IMemoryPool *memory_pool, OID oid);

			// does attribute number correspond to a transaction visibility attribute
			static 
			BOOL FTransactionVisibilityAttribute(INT attrnum);
			
			// does relation type have system columns
			static
			BOOL FHasSystemColumns(char	cRelKind);
			
			// translate Optimizer comparison types to GPDB
			static
			ULONG GetComparisonType(IMDType::ECmpType ecmpt);
			
			// retrieve the opfamilies mdids for the given scalar op
			static
			MdidPtrArray *PdrgpmdidScOpOpFamilies(IMemoryPool *memory_pool, IMDId *pmdidScOp);
			
			// retrieve the opfamilies mdids for the given index
			static
			MdidPtrArray *PdrgpmdidIndexOpFamilies(IMemoryPool *memory_pool, IMDId *pmdidIndex);

            // for non-leaf partition tables return the number of child partitions
            // else return 1
            static
            ULONG UlTableCount(OID oidRelation);

            // generate statistics for the system level columns
            static
            CDXLColStats *PdxlcolstatsSystemColumn
                              (
                              IMemoryPool *memory_pool,
                              OID oidRelation,
                              CMDIdColStats *mdid_col_stats,
                              CMDName *pmdnameCol,
                              OID oidAttType,
                              AttrNumber attrnum,
                              DXLBucketPtrArray *stats_bucket_dxl_array,
                              CDouble dRows
                              );
		public:
			// retrieve a metadata object from the relcache
			static
			IMDCacheObject *Pimdobj(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// retrieve a relation from the relcache
			static
			IMDRelation *Pmdrel(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// add system columns (oid, tid, xmin, etc) in table descriptors
			static
			void AddSystemColumns(IMemoryPool *memory_pool, MDColumnPtrArray *mdcol_array, Relation rel, BOOL fAOTable);

			// retrieve an index from the relcache
			static
			IMDIndex *Pmdindex(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdidIndex);

			// retrieve a check constraint from the relcache
			static
			CMDCheckConstraintGPDB *Pmdcheckconstraint(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// populate the attribute number to position mapping
			static
			ULONG *PulAttnoPositionMap(IMemoryPool *memory_pool, const IMDRelation *pmdrel, ULONG ulRgSize);

			// return the position of a given attribute number
			static
			ULONG UlPosition(INT iAttno, ULONG *pul);

			// retrieve a type from the relcache
			static
			IMDType *Pmdtype(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a scalar operator from the relcache
			static
			CMDScalarOpGPDB *Pmdscop(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a function from the relcache
			static
			CMDFunctionGPDB *Pmdfunc(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve an aggregate from the relcache
			static
			CMDAggregateGPDB *Pmdagg(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a trigger from the relcache
			static
			CMDTriggerGPDB *Pmdtrigger(IMemoryPool *memory_pool, IMDId *pmdid);
			
			// translate GPDB comparison type
			static
			IMDType::ECmpType ParseCmpType(ULONG ulCmpt);
			
			// get the distribution policy of the relation
			static
			IMDRelation::Ereldistrpolicy GetRelDistribution(GpPolicy *pgppolicy);
	};
}



#endif // !GPDXL_CTranslatorRelcacheToDXL_H

// EOF
