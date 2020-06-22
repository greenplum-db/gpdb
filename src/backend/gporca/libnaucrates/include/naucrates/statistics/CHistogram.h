//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CHistogram.h
//
//	@doc:
//		Histogram implementation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CHistogram_H
#define GPNAUCRATES_CHistogram_H

#include "gpos/base.h"
#include "naucrates/statistics/CBucket.h"
#include "naucrates/statistics/CStatsPred.h"

namespace gpopt
{
	class CColRef;
	class CStatisticsConfig;
	class CColumnFactory;
}

namespace gpnaucrates
{
	// type definitions
	// array of doubles
	typedef CDynamicPtrArray<CDouble, CleanupDelete> CDoubleArray;

	//---------------------------------------------------------------------------
	//	@class:
	//		CHistogram
	//
	//	@doc:
	//
	//---------------------------------------------------------------------------
	class CHistogram
	{

		// hash map from column id to a histogram
		typedef CHashMap<ULONG, CHistogram, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
							CleanupDelete<ULONG>, CleanupDelete<CHistogram> > UlongToHistogramMap;

		// iterator
		typedef CHashMapIter<ULONG, CHistogram, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
							CleanupDelete<ULONG>, CleanupDelete<CHistogram> > UlongToHistogramMapIter;

		// hash map from column ULONG to CDouble
		typedef CHashMap<ULONG, CDouble, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
						CleanupDelete<ULONG>, CleanupDelete<CDouble> > UlongToDoubleMap;

		// iterator
		typedef CHashMapIter<ULONG, CDouble, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
						CleanupDelete<ULONG>, CleanupDelete<CDouble> > UlongToDoubleMapIter;

		private:
			// shared memory pool
			CMemoryPool *m_mp;

			// all the buckets in the histogram. This is shared among histograms,
			// so must not be modified unless we first make a new copy. We do not copy
			// histograms unless required, as it is an expensive operation in memory and time.
			CBucketArray *m_histogram_buckets;

			// well-defined histogram. if false, then bounds are unknown
			BOOL m_is_well_defined;

			// null fraction
			CDouble m_null_freq;

			// ndistinct of tuples not covered in the buckets
			CDouble m_distinct_remaining;

			// frequency of tuples not covered in the buckets
			CDouble m_freq_remaining;

			// has histogram skew been measures
			BOOL m_skew_was_measured;

			// skew estimate
			CDouble m_skew;

			// was the NDVs in histogram scaled
			BOOL m_NDVs_were_scaled;

			// is column statistics missing in the database
			BOOL m_is_col_stats_missing;

			// private copy ctor
			CHistogram(const CHistogram &);

			// private assignment operator
			CHistogram& operator=(const CHistogram &);

			// return an array buckets after applying equality filter on the histogram buckets
			CBucketArray *MakeBucketsWithEqualityFilter(CPoint *point) const;

			// return an array buckets after applying non equality filter on the histogram buckets
			CBucketArray *MakeBucketsWithInequalityFilter(CPoint *point) const;

			// less than or less than equal filter
			CHistogram *MakeHistogramLessThanOrLessThanEqualFilter(CStatsPred::EStatsCmpType stats_cmp_type, CPoint *point) const;

			// greater than or greater than equal filter
			CHistogram *MakeHistogramGreaterThanOrGreaterThanEqualFilter(CStatsPred::EStatsCmpType stats_cmp_type, CPoint *point) const;

			// equal filter
			CHistogram *MakeHistogramEqualFilter(CPoint *point) const;

			// not equal filter
			CHistogram *MakeHistogramInequalityFilter(CPoint *point) const;

			// IDF filter
			CHistogram *MakeHistogramIDFFilter(CPoint *point) const;

			// INDF filter
			CHistogram *MakeHistogramINDFFilter(CPoint *point) const;

			// equality join
			CHistogram *MakeJoinHistogramEqualityFilter(const CHistogram *histogram) const;

			// generate histogram based on NDV
			CHistogram *MakeNDVBasedJoinHistogramEqualityFilter(const CHistogram *histogram) const;

			// construct a new histogram for an INDF join predicate
			CHistogram *MakeJoinHistogramINDFFilter(const CHistogram *histogram) const;

			// accessor for n-th bucket
			CBucket *operator [] (ULONG) const;

			// compute skew estimate
			void ComputeSkew();

			// helper to add buckets from one histogram to another
			static
			void AddBuckets
					(
					CMemoryPool *mp,
					const CBucketArray *src_buckets,
					CBucketArray *dest_buckets,
					CDouble rows_old,
					CDouble rows_new,
					ULONG begin,
					ULONG end
					);

			static
			void AddBuckets
					(
					CMemoryPool *mp,
					const CBucketArray *src_buckets,
					CBucketArray *dest_buckets,
					CDouble rows,
					CDoubleArray *dest_bucket_freqs,
					ULONG begin,
					ULONG end
					);

			// helper to combine histogram buckets to reduce total buckets
			static
			CBucketArray *CombineBuckets
					(
					CMemoryPool *mp,
					CBucketArray *buckets,
					ULONG desired_num_buckets
					);

			// check if we can compute NDVRemain for JOIN histogram for the given input histograms
			static
			BOOL CanComputeJoinNDVRemain(const CHistogram *histogram1, const CHistogram *histogram2);

			// compute the effects of the NDV and frequency of the tuples not captured
			// by the histogram
			static
			void ComputeJoinNDVRemainInfo
					(
					const CHistogram *histogram1,
					const CHistogram *histogram2,
					const CBucketArray *join_buckets, // join buckets
					CDouble hist1_buckets_freq, // frequency of the buckets in input1 that contributed to the join
					CDouble hist2_buckets_freq, // frequency of the buckets in input2 that contributed to the join
					CDouble *result_distinct_remain,
					CDouble *result_freq_remain
					);


			// check if the cardinality estimation should be done only via NDVs
			static
			BOOL NeedsNDVBasedCardEstimationForEq(const CHistogram *histogram);

			BOOL IsHistogramForTextRelatedTypes() const;

			// add residual union all buckets after the merge
			ULONG AddResidualUnionAllBucket
				(
				CBucketArray *histogram_buckets,
				CBucket *bucket,
				CDouble rows_old,
				CDouble rows_new,
				BOOL bucket_is_residual,
				ULONG index
				)
				const;

			// add residual union buckets after the merge
			ULONG AddResidualUnionBucket
				(
				CBucketArray *histogram_buckets,
				CBucket *bucket,
				CDouble rows,
				BOOL bucket_is_residual,
				ULONG index,
				CDoubleArray *dest_bucket_freqs
				)
				const;

			// used to keep track of adjacent stats buckets and how similar
			// they are in terms of distribution
			struct SAdjBucketBoundary
				{
					// boundary_index 0 refers to boundary between b[0] and b[1]
					ULONG m_boundary_index;
					// similarity factor between two adjacent buckets calculated as (freq0/ndv0 - freq1/ndv1) + (freq0/width0 - freq1/width1)
					CDouble m_similarity_factor;

					SAdjBucketBoundary(
							ULONG index,
							CDouble similarity_factor
						) :
						m_boundary_index(index),
						m_similarity_factor(similarity_factor) {}

					// used for sorting in the binary heap
					CDouble DCost() { return m_similarity_factor; }
					CDouble GetCostForHeap() { return DCost(); }
				};
			typedef CDynamicPtrArray<SAdjBucketBoundary, CleanupDelete<SAdjBucketBoundary> > SAdjBucketBoundaryArray;

			// a heap keeping the k lowest-cost objects in an array of class A
			// A is a CDynamicPtrArray
			// E is the entry type of the array and it has a method CDouble DCost()
			// See https://en.wikipedia.org/wiki/Binary_heap for details
			// (with the added feature of returning only the top k).
		    template<class A, class E>
			class KHeap : public CRefCount
			{
			private:

				A *m_topk;
				CMemoryPool *m_mp;
				ULONG m_k;
				BOOL m_is_heapified;
				ULONG m_num_returned;

				// the parent index is (ix-1)/2, except for 0
				ULONG parent(ULONG ix) { return (0 < ix ? (ix-1)/2 : m_topk->Size()); }

				// children are at indexes 2*ix + 1 and 2*ix + 2
				ULONG left_child(ULONG ix)  { return 2*ix + 1; }
				ULONG right_child(ULONG ix) { return 2*ix + 2; }

				// does the parent/child exist?
				BOOL exists(ULONG ix) { return ix < m_topk->Size(); }
				// cost of an entry (this class implements a Min-Heap)
				CDouble cost(ULONG ix) { return (*m_topk)[ix]->DCost(); }

				// push node ix in the tree down into its child tree as much as needed
				void HeapifyDown(ULONG ix)
				{
					ULONG left_child_ix = left_child(ix);
					ULONG right_child_ix = right_child(ix);
					ULONG min_element_ix = ix;

					if (exists(left_child_ix) && cost(left_child_ix) < cost(ix))
						// left child is better than parent, it becomes the new candidate
						min_element_ix = left_child_ix;

					if (exists(right_child_ix) && cost(right_child_ix) < cost(min_element_ix))
						// right child is better than min(parent, left child)
						min_element_ix = right_child_ix;

					if (min_element_ix != ix)
					{
						// make the lowest of { parent, left child, right child } the new root
						m_topk->Swap(ix, min_element_ix);
						HeapifyDown(min_element_ix);
					}
				}

				// pull node ix in the tree up as much as needed
				void HeapifyUp(ULONG ix)
				{
					ULONG parent_ix = parent(ix);

					if (!exists(parent_ix))
						return;

					if (cost(ix) < cost(parent_ix))
					{
						m_topk->Swap(ix, parent_ix);
						HeapifyUp(parent_ix);
					}
				}

				// Convert the array into a heap, heapify-down all interior nodes of the tree, bottom-up.
				// Note that we keep all the entries, not just the top k, since our k-heaps are short-lived.
				// You can only retrieve the top k with RemoveBestElement(), though.
				void Heapify()
				{
					// the parent of the last node is the last node in the tree that is a parent
					ULONG start_ix = parent(m_topk->Size()-1);

					// now work our way up to the root, calling HeapifyDown
					for (ULONG ix=start_ix; exists(ix); ix--)
						HeapifyDown(ix);

					m_is_heapified = true;
				}

			public:

				KHeap(CMemoryPool *mp, ULONG k)
				:
				m_mp(mp),
				m_k(k),
				m_is_heapified(false),
				m_num_returned(0)
				{
					m_topk = GPOS_NEW(m_mp) A(m_mp);
				}

				~KHeap()
				{
					m_topk->Release();
				}

				void Insert(E *elem)
				{
					GPOS_ASSERT(NULL != elem);
					// since the cost may change as we find more expressions in the group,
					// we just append to the array now and heapify at the end
					GPOS_ASSERT(!m_is_heapified);
					m_topk->Append(elem);

					// this is dead code at the moment, but other users might want to
					// heapify and then insert additional items
					if (m_is_heapified)
					{
						HeapifyUp(m_topk->Size()-1);
					}
				}

				// remove the next of the top k elements, sorted ascending by cost
				E *RemoveBestElement()
				{
					if (0 == m_topk->Size() || m_k <= m_num_returned)
					{
						return NULL;
					}

					m_num_returned++;

					return RemoveNextElement();
				}

				// remove the next best element, without the top k limit
				E *RemoveNextElement()
				{
					if (0 == m_topk->Size())
					{
						return NULL;
					}

					if (!m_is_heapified)
						Heapify();

					// we want to remove and return the root of the tree, which is the best element

					// first, swap the root with the last element in the array
					m_topk->Swap(0, m_topk->Size()-1);

					// now remove the new last element, which is the real root
					E * result = m_topk->RemoveLast();

					// then push the new first element down to the correct place
					HeapifyDown(0);

					return result;
				}

				ULONG Size()
				{
					return m_topk->Size();
				}

				BOOL IsLimitExceeded()
				{
					return m_topk->Size() + m_num_returned > m_k;
				}

				void Clear()
				{
					m_topk->Clear();
					m_is_heapified = false;
					m_num_returned = 0;
				}

			};
		public:

			// ctors
			explicit
			CHistogram(CMemoryPool *mp, CBucketArray *histogram_buckets, BOOL is_well_defined = true);

			explicit
			CHistogram(CMemoryPool *mp, BOOL is_well_defined = true);

			CHistogram
					(
					CMemoryPool *mp,
					CBucketArray *histogram_buckets,
					BOOL is_well_defined,
					CDouble null_freq,
					CDouble distinct_remaining,
					CDouble freq_remaining,
					BOOL is_col_stats_missing = false
					);

			// set null frequency
			void SetNullFrequency(CDouble null_freq);

			// set information about the scaling of NDVs
			void SetNDVScaled()
			{
				m_NDVs_were_scaled = true;
			}

			// have the NDVs been scaled
			BOOL WereNDVsScaled() const
			{
				return m_NDVs_were_scaled;
			}

			// filter by comparing with point
			CHistogram *MakeHistogramFilter
						(
						CStatsPred::EStatsCmpType stats_cmp_type,
						CPoint *point
						)
						const;

			// filter by comparing with point and normalize
			CHistogram *MakeHistogramFilterNormalize
						(
						CStatsPred::EStatsCmpType stats_cmp_type,
						CPoint *point,
						CDouble *scale_factor
						)
						const;

			// join with another histogram
			CHistogram *MakeJoinHistogram
						(
						CStatsPred::EStatsCmpType stats_cmp_type,
						const CHistogram *histogram
						)
						const;

			// LASJ with another histogram
			CHistogram *MakeLASJHistogram
						(
						CStatsPred::EStatsCmpType stats_cmp_type,
						const CHistogram *histogram
						)
						const;

			// join with another histogram and normalize it.
			// If the join is not an equality join the function returns an empty histogram
			CHistogram *MakeJoinHistogramNormalize
						(
						CStatsPred::EStatsCmpType stats_cmp_type,
						CDouble rows,
						const CHistogram *other_histogram,
						CDouble rows_other,
						CDouble *scale_factor
						)
						const;

			// scale factor of inequality (!=) join
			CDouble GetInequalityJoinScaleFactor
						(
						CDouble rows,
						const CHistogram *other_histogram,
						CDouble rows_other
						)
						const;

			// left anti semi join with another histogram and normalize
			CHistogram *MakeLASJHistogramNormalize
						(
						CStatsPred::EStatsCmpType stats_cmp_type,
						CDouble rows,
						const CHistogram *other_histogram,
						CDouble *scale_factor,
						BOOL DoIgnoreLASJHistComputation // except for the case of LOJ cardinality estimation this flag is always
						                                // "true" since LASJ stats computation is very aggressive
						)
						const;

			// group by and normalize
			CHistogram *MakeGroupByHistogramNormalize
						(
						CDouble rows,
						CDouble *scale_factor
						)
						const;

			// union all and normalize
			CHistogram *MakeUnionAllHistogramNormalize
						(
						CDouble rows,
						const CHistogram *other_histogram,
						CDouble rows_other
						)
						const;

			// union and normalize
			CHistogram *MakeUnionHistogramNormalize
						(
						CDouble rows,
						const CHistogram *other_histogram,
						CDouble rows_other,
						CDouble *num_output_rows
						)
						const;

			// cleanup residual buckets
			void CleanupResidualBucket(CBucket *bucket, BOOL bucket_is_residual) const;

			// get the next bucket for union / union all
			CBucket *GetNextBucket
					(
					const CHistogram *histogram,
					CBucket *new_bucket,
					BOOL *target_bucket_is_residual,
					ULONG *current_bucket_index
					)
					const;

			// number of buckets
			ULONG GetNumBuckets() const
			{
				GPOS_ASSERT(m_histogram_buckets != NULL);
				return m_histogram_buckets->Size();
			}

			// buckets accessor
			const CBucketArray *GetBuckets() const
			{
				return m_histogram_buckets;
			}

			// well defined
			BOOL IsWellDefined() const
			{
				return m_is_well_defined;
			}

			BOOL ContainsOnlySingletonBuckets() const;

			// is the column statistics missing in the database
			BOOL IsColStatsMissing() const
			{
				return m_is_col_stats_missing;
			}

			// print function
			virtual
			IOstream &OsPrint(IOstream &os) const;

#ifdef GPOS_DEBUG
			void DbgPrint() const;
#endif

			// total frequency from buckets and null fraction
			CDouble GetFrequency() const;

			// total number of distinct values
			CDouble GetNumDistinct() const;

			// is histogram well formed
			BOOL IsValid() const;

			// return copy of histogram
			CHistogram *CopyHistogram() const;

			// destructor
			virtual
			~CHistogram()
			{
				m_histogram_buckets->Release();
			}

			// normalize histogram and return scaling factor
			CDouble NormalizeHistogram();

			// is histogram normalized
			BOOL IsNormalized() const;

			// translate the histogram into a derived column stats
			CDXLStatsDerivedColumn *TranslateToDXLDerivedColumnStats
				(
				CMDAccessor *md_accessor,
				ULONG colid,
				CDouble width
				)
				const;

			// randomly pick a bucket index
			ULONG GetRandomBucketIndex(ULONG *seed) const;

			// estimate of data skew
			CDouble GetSkew()
			{
				if (!m_skew_was_measured)
				{
					ComputeSkew();
				}

				return m_skew;
			}

			// accessor of null fraction
			CDouble GetNullFreq() const
			{
				return m_null_freq;
			}

			// accessor of remaining number of tuples
			CDouble GetDistinctRemain() const
			{
				return m_distinct_remaining;
			}

			// accessor of remaining frequency
			CDouble GetFreqRemain() const
			{
				return m_freq_remaining;
			}

			// check if histogram is empty
			BOOL IsEmpty() const;

			// cap the total number of distinct values (NDVs) in buckets to the number of rows
			void CapNDVs(CDouble rows);

			// is comparison type supported for filters for text columns
			static
			BOOL IsOpSupportedForTextFilter(CStatsPred::EStatsCmpType stats_cmp_type);

			// is comparison type supported for filters
			static
			BOOL IsOpSupportedForFilter(CStatsPred::EStatsCmpType stats_cmp_type);

			// is the join predicate's comparison type supported
			static
			BOOL JoinPredCmpTypeIsSupported(CStatsPred::EStatsCmpType stats_cmp_type);

			// create the default histogram for a given column reference
			static
			CHistogram *MakeDefaultHistogram(CMemoryPool *mp, CColRef *col_ref, BOOL is_empty);

			// create the default non empty histogram for a boolean column
			static
			CHistogram *MakeDefaultBoolHistogram(CMemoryPool *mp);

			// helper method to append histograms from one map to the other
			static
			void AddHistograms(CMemoryPool *mp, UlongToHistogramMap *src_histograms, UlongToHistogramMap *dest_histograms);

			// add dummy histogram buckets and column width for the array of columns
			static
			void AddDummyHistogramAndWidthInfo
				(
				CMemoryPool *mp,
				CColumnFactory *col_factory,
												  UlongToHistogramMap *output_histograms,
												  UlongToDoubleMap *output_col_widths,
				const ULongPtrArray *columns,
				BOOL is_empty
				);

			// add dummy histogram buckets for the columns in the input histogram
			static
			void AddEmptyHistogram(CMemoryPool *mp, UlongToHistogramMap *output_histograms, UlongToHistogramMap *input_histograms);

			// create a deep copy of m_histogram_buckets
			static
			CBucketArray* DeepCopyHistogramBuckets
				(
				CMemoryPool *mp,
				const CBucketArray *buckets
				);

			// default histogram selectivity
			static const CDouble DefaultSelectivity;

			// minimum number of distinct values in a column
			static const CDouble MinDistinct;

			// default scale factor when there is no filtering of input
			static const CDouble NeutralScaleFactor;

			// default Null frequency
			static const CDouble DefaultNullFreq;

			// default NDV remain
			static const CDouble DefaultNDVRemain;

			// default frequency of NDV remain
			static const CDouble DefaultNDVFreqRemain;
	}; // class CHistogram

}

#endif // !GPNAUCRATES_CHistogram_H

// EOF
