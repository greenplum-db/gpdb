-- We rely on pageinspect to perform white-box testing for summarization.
-- White-box tests are necessary to ensure that summarization is done
-- successfully (to avoid cases where ranges have brin data tuples without
-- values or where the range is not covered by the revmap etc)
CREATE EXTENSION pageinspect;

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
    generate_series(0, blocks('brin_range_extended_heap_i_idx') - 1) blkno;
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
    generate_series(0, blocks('brin_range_extended_heap_i_idx') - 1) blkno;
1U: SELECT * FROM brin_revmap_data(get_raw_page('brin_range_extended_heap_i_idx', 1))
    WHERE pages != '(0,0)' order by 1;
1U: SELECT * FROM brin_page_items(get_raw_page('brin_range_extended_heap_i_idx', 2),
                                  'brin_range_extended_heap_i_idx') ORDER BY blknum, attnum;

DROP EXTENSION pageinspect;
