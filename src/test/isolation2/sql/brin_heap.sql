-- We rely on pageinspect to perform white-box testing for summarization.
-- White-box tests are necessary to ensure that summarization is done
-- successfully (to avoid cases where ranges have brin data tuples without
-- values or where the range is not covered by the revmap etc)

-- Turn off sequential scans to force usage of BRIN indexes for scans.
SET enable_seqscan TO off;

-- Ensure that we can summarize the last partial range in case it was extended
-- by another transaction, while summarization was in flight.

CREATE TABLE brin_range_extended_heap(i int) USING heap;
CREATE INDEX ON brin_range_extended_heap USING brin(i) WITH (pages_per_range=5);

-- Insert 9 blocks of data on 1 QE; 8 blocks full, 1 block with 1 tuple.
SELECT populate_pages('brin_range_extended_heap', 1, tid '(8, 0)');

-- Set up to suspend execution when will attempt to summarize the final partial
-- range below: [5, 8].
SELECT gp_inject_fault('summarize_last_partial_range', 'suspend', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1&: SELECT brin_summarize_new_values('brin_range_extended_heap_i_idx');

SELECT gp_wait_until_triggered_fault('summarize_last_partial_range', 1, dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: We should only have 1 (placeholder) tuple inserted (for the final
-- partial range [5, 8]).
1U: SELECT blkno, brin_page_type(get_raw_page('brin_range_extended_heap_i_idx', blkno)) FROM
    generate_series(0, nblocks('brin_range_extended_heap_i_idx') - 1) blkno;
1U: SELECT * FROM brin_revmap_data(get_raw_page('brin_range_extended_heap_i_idx', 1))
    WHERE pages != '(0,0)' order by 1;
1U: SELECT * FROM brin_page_items(get_raw_page('brin_range_extended_heap_i_idx', 2),
                                  'brin_range_extended_heap_i_idx') ORDER BY blknum, attnum;

-- Extend the last partial range by 1 block.
SELECT populate_pages('brin_range_extended_heap', 20, tid '(9, 0)');

SELECT gp_inject_fault('summarize_last_partial_range', 'reset', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1<:

-- Sanity: We should have the final full range [5, 9] summarized with both
-- existing tuples and the tuples from the concurrent insert.
1U: SELECT blkno, brin_page_type(get_raw_page('brin_range_extended_heap_i_idx', blkno)) FROM
    generate_series(0, nblocks('brin_range_extended_heap_i_idx') - 1) blkno;
1U: SELECT * FROM brin_revmap_data(get_raw_page('brin_range_extended_heap_i_idx', 1))
    WHERE pages != '(0,0)' order by 1;
1U: SELECT * FROM brin_page_items(get_raw_page('brin_range_extended_heap_i_idx', 2),
                                  'brin_range_extended_heap_i_idx') ORDER BY blknum, attnum;


-- Test build/summarize with aborted rows.

CREATE TABLE brin_abort_heap(i int);
CREATE INDEX ON brin_abort_heap USING brin(i) WITH (pages_per_range=1);
BEGIN;
-- Create 3 blocks all on 1 QE, in 1 aoseg: 2 blocks full, 1 block with 1 tuple.
SELECT populate_pages('brin_abort_heap', 1, tid '(2, 0)');
ABORT;

-- Sanity: There is 1 revmap page and 1 data page, with 1 range (summarized).
-- This first range being summarized highlights a difference with AO/CO tables.
1U: SELECT blkno, brin_page_type(get_raw_page('brin_abort_heap_i_idx', blkno)) FROM
    generate_series(0, nblocks('brin_abort_heap_i_idx') - 1) blkno;
1U: SELECT * FROM brin_revmap_data(get_raw_page('brin_abort_heap_i_idx', 1))
   WHERE pages != '(0,0)' order by 1;
1U: SELECT * FROM brin_page_items(get_raw_page('brin_abort_heap_i_idx', 2),
                                  'brin_abort_heap_i_idx') ORDER BY blknum, attnum;

-- Summarize over the aborted rows.
SELECT brin_summarize_new_values('brin_abort_heap_i_idx');

-- Sanity: There is 1 revmap page and 1 data page, with 3 ranges. The two new
-- ranges have empty range tuples as a result of explicit summarization over
-- aborted tuples.
1U: SELECT blkno, brin_page_type(get_raw_page('brin_abort_heap_i_idx', blkno)) FROM
    generate_series(0, nblocks('brin_abort_heap_i_idx') - 1) blkno;
1U: SELECT * FROM brin_revmap_data(get_raw_page('brin_abort_heap_i_idx', 1))
    WHERE pages != '(0,0)' order by 1;
1U: SELECT * FROM brin_page_items(get_raw_page('brin_abort_heap_i_idx', 2),
                                  'brin_abort_heap_i_idx') ORDER BY blknum, attnum;

-- Sanity: Scan should only return the 1st block and ignore the blocks for which
-- we have the empty tuples, in the tidbitmap.
SELECT gp_inject_fault_infinite('brin_bitmap_page_added', 'skip', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT count(*) FROM brin_abort_heap WHERE i = 1;
SELECT gp_inject_fault('brin_bitmap_page_added', 'status', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('brin_bitmap_page_added', 'reset', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Now, add some committed rows.
SELECT populate_pages('brin_abort_heap', 20, tid '(3, 0)');

-- Summarize to include the committed rows.
SELECT brin_summarize_new_values('brin_abort_heap_i_idx');

-- Sanity: There is 1 revmap page and 1 data page, with 4 ranges. The first range
-- and the last two ranges (covering the committed rows) have non-empty tuples.
1U: SELECT blkno, brin_page_type(get_raw_page('brin_abort_heap_i_idx', blkno)) FROM
    generate_series(0, nblocks('brin_abort_heap_i_idx') - 1) blkno;
1U: SELECT * FROM brin_revmap_data(get_raw_page('brin_abort_heap_i_idx', 1))
    WHERE pages != '(0,0)' order by 1;
1U: SELECT * FROM brin_page_items(get_raw_page('brin_abort_heap_i_idx', 2),
                                  'brin_abort_heap_i_idx') ORDER BY blknum, attnum;

-- Sanity: Scan should only return the 2 blocks matching the predicate, in the tidbitmap.
SELECT gp_inject_fault_infinite('brin_bitmap_page_added', 'skip', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT count(*) FROM brin_abort_heap WHERE i = 20;
SELECT gp_inject_fault('brin_bitmap_page_added', 'status', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('brin_bitmap_page_added', 'reset', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Drop and re-create the index to test build.
DROP INDEX brin_abort_heap_i_idx;
CREATE INDEX ON brin_abort_heap USING brin(i) WITH (pages_per_range=1);

-- Sanity: There is 1 revmap page and 1 data page, with 4 ranges. Only the last
-- two ranges (covering the committed rows) have non-empty tuples.
1U: SELECT blkno, brin_page_type(get_raw_page('brin_abort_heap_i_idx', blkno)) FROM
    generate_series(0, nblocks('brin_abort_heap_i_idx') - 1) blkno;
1U: SELECT * FROM brin_revmap_data(get_raw_page('brin_abort_heap_i_idx', 1))
    WHERE pages != '(0,0)' order by 1;
1U: SELECT * FROM brin_page_items(get_raw_page('brin_abort_heap_i_idx', 2),
                                  'brin_abort_heap_i_idx') ORDER BY blknum, attnum;

-- Sanity: Scan should only return the 2 blocks matching the predicate, in the tidbitmap.
SELECT gp_inject_fault_infinite('brin_bitmap_page_added', 'skip', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT count(*) FROM brin_abort_heap WHERE i = 20;
SELECT gp_inject_fault('brin_bitmap_page_added', 'status', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('brin_bitmap_page_added', 'reset', dbid)
FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

RESET enable_seqscan;
