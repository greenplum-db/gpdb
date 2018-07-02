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
				IMDFunction::EFuncStbl m_stability;

				// function data access
				IMDFunction::EFuncDataAcc m_access;

				// is function strict?
				BOOL m_is_strict;

				// can the function return multiple rows?
				BOOL m_returns_set;

			public:

				// ctor
				SFuncProps
					(
					OID oid,
					IMDFunction::EFuncStbl stability,
					IMDFunction::EFuncDataAcc access,
					BOOL is_strict,
					BOOL ReturnsSet
					)
					:
					m_oid(oid),
					m_stability(stability),
					m_access(access),
					m_is_strict(is_strict),
					m_returns_set(ReturnsSet)
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
				IMDFunction::EFuncStbl GetStability() const
				{
					return m_stability;
				}

				// return data access property
				IMDFunction::EFuncDataAcc GetDataAccess() const
				{
					return m_access;
				}

				// is function strict?
				BOOL IsStrict() const
				{
					return m_is_strict;
				}

				// does function return set?
				BOOL ReturnsSet() const
				{
					return m_returns_set;
				}

			}; // struct SFuncProps

			// array of function properties map
			static
			const SFuncProps m_func_props[];

			// lookup function properties
			static
			void LookupFuncProps
				(
				OID oidFunc,
				IMDFunction::EFuncStbl *stability, // output: function stability
				IMDFunction::EFuncDataAcc *access, // output: function data access
				BOOL *is_strict, // output: is function strict?
				BOOL *ReturnsSet // output: does function return set?
				);

			// check and fall back for unsupported relations
			static
			void CheckUnsupportedRelation(OID oidRel);

			// get type name from the relcache
			static
			CMDName *GetTypeName(IMemoryPool *memory_pool, IMDId *pmdid);

			// get function stability property from the GPDB character representation
			static
			CMDFunctionGPDB::EFuncStbl GetFuncStability(CHAR c);

			// get function data access property from the GPDB character representation
			static
			CMDFunctionGPDB::EFuncDataAcc GetEFuncDataAccess(CHAR c);

			// get type of aggregate's intermediate result from the relcache
			static
			IMDId *RetrieveAggIntermediateResultType(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a GPDB metadata object from the relcache
			static
			IMDCacheObject *RetreiveMDObjGPDB(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// retrieve relstats object from the relcache
			static
			IMDCacheObject *RetrieveRelStats(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve column stats object from the relcache
			static
			IMDCacheObject *RetrieveColStats(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// retrieve cast object from the relcache
			static
			IMDCacheObject *RetreiveCast(IMemoryPool *memory_pool, IMDId *pmdid);
			
			// retrieve scalar comparison object from the relcache
			static
			IMDCacheObject *RetreiveScCmp(IMemoryPool *memory_pool, IMDId *pmdid);

			// transform GPDB's MCV information to optimizer's histogram structure
			static
			CHistogram *TransformMcvToOrcaHistogram
								(
								IMemoryPool *memory_pool,
								const IMDType *pmdtype,
								const Datum *pdrgdatumMCVValues,
								const float4 *pdrgfMCVFrequencies,
								ULONG ulNumMCVValues
								);

			// transform GPDB's hist information to optimizer's histogram structure
			static
			CHistogram *TransformHistToOrcaHistogram
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
			DXLBucketPtrArray *TransformHistogramToDXLBucketArray
								(
								IMemoryPool *memory_pool,
								const IMDType *pmdtype,
								const CHistogram *phist
								);

			// transform stats from pg_stats form to optimizer's preferred form
			static
			DXLBucketPtrArray *TransformStatsToDXLBucketArray
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
			void RetrievePartKeysAndTypes(IMemoryPool *memory_pool, Relation rel, OID oid, ULongPtrArray **pdrgpulPartKeys, CharPtrArray **pdrgpszPartTypes);

			// get keysets for relation
			static
			ULongPtrArray2D *RetrieveRelKeysets(IMemoryPool *memory_pool, OID oid, BOOL fAddDefaultKeys, BOOL fPartitioned, ULONG *pulMapping);

			// storage type for a relation
			static
			IMDRelation::Erelstoragetype RetrieveRelStorageType(CHAR cStorageType);

			// fix frequencies if they add up to more than 1.0
			static
			void NormalizeFrequencies(float4 *pdrgf, ULONG length, CDouble *pdNullFrequency);

			// get the relation columns
			static
			MDColumnPtrArray *RetrieveRelColumns(IMemoryPool *memory_pool, CMDAccessor *md_accessor, Relation rel, IMDRelation::Erelstoragetype rel_storage_type);

			// return the dxl representation of the column's default value
			static
			CDXLNode *GetDefaultColumnValue(IMemoryPool *memory_pool, CMDAccessor *md_accessor, TupleDesc rd_att, AttrNumber attrno);


			// get the distribution columns
			static
			ULongPtrArray *RetrieveRelDistrbutionCols(IMemoryPool *memory_pool, GpPolicy *pgppolicy, MDColumnPtrArray *mdcol_array, ULONG size);

			// construct a mapping GPDB attnos -> position in the column array
			static
			ULONG *ConstructAttnoMapping(IMemoryPool *memory_pool, MDColumnPtrArray *mdcol_array, ULONG ulMaxCols);

			// check if index is supported
			static
			BOOL IsIndexSupported(Relation relIndex);
			
			// retrieve index info list of partitioned table
			static
			List *RetrievePartTableIndexInfo(Relation rel);
			 
			// compute the array of included columns
			static
			ULongPtrArray *ComputeIncludedCols(IMemoryPool *memory_pool, const IMDRelation *md_rel);
			
			// is given level included in the default partitions
			static 
			BOOL IsDefaultPartition(List *plDefaultLevels, ULONG ulLevel);
			
			// retrieve part constraint for index
			static
			CMDPartConstraintGPDB *RetrievePartConstraintForIndex
				(
				IMemoryPool *memory_pool, 
				CMDAccessor *md_accessor, 
				const IMDRelation *md_rel, 
				Node *pnodePartCnstr,
				ULongPtrArray *level_with_default_part_array,
				BOOL is_unbounded
				);

			// retrieve part constraint for relation
			static
			CMDPartConstraintGPDB *RetrievePartConstraintForRel(IMemoryPool *memory_pool, CMDAccessor *md_accessor, OID oidRel, MDColumnPtrArray *mdcol_array, BOOL fhasIndex);

			// retrieve part constraint from a GPDB node
			static
			CMDPartConstraintGPDB *RetrievePartConstraintFromNode
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
			CMDName *GetRelName(IMemoryPool *memory_pool, Relation rel);

			// return the index info list defined on the given relation
			static
			MDIndexInfoPtrArray *RetrieveRelIndexInfo(IMemoryPool *memory_pool, Relation rel);

			// return index info list of indexes defined on a partitoned table
			static
			MDIndexInfoPtrArray *RetrieveRelIndexInfoForPartTable(IMemoryPool *memory_pool, Relation relRoot);

			// return index info list of indexes defined on regular, external tables or leaf partitions
			static
			MDIndexInfoPtrArray *RetrieveRelIndexInfoForNonPartTable(IMemoryPool *memory_pool, Relation rel);

			// retrieve an index over a partitioned table from the relcache
			static
			IMDIndex *RetrievePartTableIndex(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdidIndex, const IMDRelation *md_rel, LogicalIndexes *plind);
			
			// lookup an index given its id from the logical indexes structure
			static
			LogicalIndexInfo *LookupLogicalIndexById(LogicalIndexes *plind, OID oid);
			
			// construct an MD cache index object given its logical index representation
			static
			IMDIndex *RetrievePartTableIndex(IMemoryPool *memory_pool, CMDAccessor *md_accessor, LogicalIndexInfo *pidxinfo, IMDId *pmdidIndex, const IMDRelation *md_rel);

			// return the triggers defined on the given relation
			static
			MdidPtrArray *RetrieveRelTriggers(IMemoryPool *memory_pool, Relation rel);

			// return the check constraints defined on the relation with the given oid
			static
			MdidPtrArray *RetrieveRelCheckConstraints(IMemoryPool *memory_pool, OID oid);

			// does attribute number correspond to a transaction visibility attribute
			static 
			BOOL IsTransactionVisibilityAttribute(INT attrnum);
			
			// does relation type have system columns
			static
			BOOL RelHasSystemColumns(char	cRelKind);
			
			// translate Optimizer comparison types to GPDB
			static
			ULONG GetComparisonType(IMDType::ECmpType ecmpt);
			
			// retrieve the opfamilies mdids for the given scalar op
			static
			MdidPtrArray *RetrieveScOpOpFamilies(IMemoryPool *memory_pool, IMDId *pmdidScOp);
			
			// retrieve the opfamilies mdids for the given index
			static
			MdidPtrArray *RetrieveIndexOpFamilies(IMemoryPool *memory_pool, IMDId *pmdidIndex);

            // for non-leaf partition tables return the number of child partitions
            // else return 1
            static
            ULONG RetrieveNumChildPartitions(OID oidRelation);

            // generate statistics for the system level columns
            static
            CDXLColStats *GenerateStatsForSystemCols
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
			IMDCacheObject *RetrieveObject(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// retrieve a relation from the relcache
			static
			IMDRelation *RetrieveRel(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// add system columns (oid, tid, xmin, etc) in table descriptors
			static
			void AddSystemColumns(IMemoryPool *memory_pool, MDColumnPtrArray *mdcol_array, Relation rel, BOOL fAOTable);

			// retrieve an index from the relcache
			static
			IMDIndex *RetrieveIndex(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdidIndex);

			// retrieve a check constraint from the relcache
			static
			CMDCheckConstraintGPDB *RetrieveCheckConstraints(IMemoryPool *memory_pool, CMDAccessor *md_accessor, IMDId *pmdid);

			// populate the attribute number to position mapping
			static
			ULONG *PopulateAttnoPositionMap(IMemoryPool *memory_pool, const IMDRelation *md_rel, ULONG ulRgSize);

			// return the position of a given attribute number
			static
			ULONG GetAttributePosition(INT attno, ULONG *pul);

			// retrieve a type from the relcache
			static
			IMDType *RetrieveType(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a scalar operator from the relcache
			static
			CMDScalarOpGPDB *RetrieveScOp(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a function from the relcache
			static
			CMDFunctionGPDB *RetrieveFunc(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve an aggregate from the relcache
			static
			CMDAggregateGPDB *RetrieveAgg(IMemoryPool *memory_pool, IMDId *pmdid);

			// retrieve a trigger from the relcache
			static
			CMDTriggerGPDB *RetrieveTrigger(IMemoryPool *memory_pool, IMDId *pmdid);
			
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
