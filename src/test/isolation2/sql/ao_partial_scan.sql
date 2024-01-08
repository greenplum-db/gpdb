-- This test file asserts the correctness and efficiency of partial scans on
-- AO/CO tables, using BRIN partial range summarization as a test driver. The
-- efficiency is tougher to assert due to the impedance mismatch between logical
-- heap blocks and physical varblocks. We use a fault point to see how many
-- varblocks get scanned and verify it against the range of block directory
-- entries that should be involved.

--------------------------------------------------------------------------------
----                            ao_row tables
--------------------------------------------------------------------------------

CREATE TABLE ao_partial_scan1(i int, j int) USING ao_row;

--------------------------------------------------------------------------------
-- Scenario 1: Starting block number of scans map to block directory entries,
-- across multiple minipages, corresponding to multiple segfiles.
--------------------------------------------------------------------------------

-- Create a couple of seg files, spanning a couple of minipages each.
1: BEGIN;
2: BEGIN;
1: INSERT INTO ao_partial_scan1 SELECT 1,j FROM generate_series(1, 300000) j;
2: INSERT INTO ao_partial_scan1 SELECT 20,-j FROM generate_series(1, 300000) j;
1: COMMIT;
2: COMMIT;

-- Doing an index build will result in scanning the relation whole.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON ao_partial_scan1 USING brin(j) WITH (pages_per_range = 3);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- We have 2 minipages each for the 2 segnos.
1U: SELECT tupleid, segno, count(entry_no) AS num_entries, sum(row_count) AS total_rowcount
  FROM gp_toolkit.__gp_aoblkdir('ao_partial_scan1') GROUP BY tupleid, segno ORDER BY 1,2;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan1_j_idx', 2), 'ao_partial_scan1_j_idx');

-- Now desummarize a few ranges.
1U: SELECT brin_desummarize_range('ao_partial_scan1_j_idx', 33554432);
1U: SELECT brin_desummarize_range('ao_partial_scan1_j_idx', 33554438);
1U: SELECT brin_desummarize_range('ao_partial_scan1_j_idx', 33554441);
1U: SELECT brin_desummarize_range('ao_partial_scan1_j_idx', 67108867);

-- Now summarize these desummarized ranges piecemeal and check that we scan only
-- a subset of the blocks each time.

-- Range scans beginning at 33554432 and 33554438 will span 55 block directory
-- entries (55 varblocks).
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('ao_partial_scan1')
    WHERE first_row_no < ((33554435 - 33554432) * 32768) AND segno = 1 ORDER BY 1,2,3,4,5;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('ao_partial_scan1_j_idx', 33554432);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan1_j_idx', 2), 'ao_partial_scan1_j_idx');

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('ao_partial_scan1_j_idx', 33554438);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan1_j_idx', 2), 'ao_partial_scan1_j_idx');

-- Range scan beginning at 33554441 maps to the 2nd block directory row's
-- entry_no = 1. We will scan all 4 subsequent varblocks and 1 more varblock
-- in the next segfile. The extra varblock will be scanned and the first tuple
-- returned will be outside the range, and discarded.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('ao_partial_scan1')
  WHERE tupleid = '(0,2)' AND entry_no >= 1 ORDER BY 1,2,3,4,5;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('ao_partial_scan1_j_idx', 33554441);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan1_j_idx', 2), 'ao_partial_scan1_j_idx');

-- A similar 55 blocks can be expected from scanning the first range in seg2.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('ao_partial_scan1_j_idx', 67108867);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan1_j_idx', 2), 'ao_partial_scan1_j_idx');

--------------------------------------------------------------------------------
-- Scenario 2: Starting block number of scan maps to hole at the end of the
-- minipage (after the last entry).
--------------------------------------------------------------------------------
CREATE TABLE ao_partial_scan2(i int, j int) USING ao_row;
-- Fill 1 logical heap block with committed rows.
INSERT INTO ao_partial_scan2 SELECT 1, j FROM generate_series(1, 32767) j;
-- Now add some aborted rows at the end of the segfile, resulting in a hole at
-- the end of the minipage.
BEGIN;
INSERT INTO ao_partial_scan2 SELECT 20, j FROM generate_series(1, 32768) j;
ABORT;

-- Doing an index build will result in scanning the committed rows only.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON ao_partial_scan2 USING brin(i) WITH (pages_per_range = 1);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- We have 1 minipage with 19 entries, meaning that there are 19 varblocks in
-- the table with committed rows.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('ao_partial_scan2')  ORDER BY 1,2,3,4,5;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan2_i_idx', 2), 'ao_partial_scan2_i_idx');

-- Now desummarize the first range of committed rows.
1U: SELECT brin_desummarize_range('ao_partial_scan2_i_idx', 33554432);

-- Summarizing the first range now should only scan the committed rows. (same
-- as the index build). In other words, we will scan an equal number of
-- varblocks as there are block directory entries, starting from entry_no = 0.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('ao_partial_scan2_i_idx', 33554432);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan2_i_idx', 2), 'ao_partial_scan2_i_idx');

-- The start of 33554433 falls into a hole at the end of the segfile. In this
-- case we will start our scan from the last block directory entry and will
-- read that one varblock.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('ao_partial_scan2_i_idx', 33554433);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan2_i_idx', 2), 'ao_partial_scan2_i_idx');

--------------------------------------------------------------------------------
-- Scenario 3: Starting block number of scan maps to hole at the start of the
-- segfile (and before the first entry of the first minipage).
--------------------------------------------------------------------------------
CREATE TABLE ao_partial_scan3(i int, j int) USING ao_row;
-- Create a hole with 1 logical heap block worth of aborted rows.
BEGIN;
INSERT INTO ao_partial_scan3 SELECT 1, j FROM generate_series(1, 32767) j;
ABORT;
-- Fill the next 3 logical heap blocks with committed rows.
INSERT INTO ao_partial_scan3 SELECT 20, j FROM generate_series(1, 32768 * 3) j;

-- Doing an index build will result in scanning the committed rows only.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON ao_partial_scan3 USING brin(i) WITH (pages_per_range = 3);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- We have 1 minipage with 55 entries, meaning that there are 55 varblocks worth
-- of committed rows. The first entry's firstRowNum reveals the hole at the
-- beginning.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('ao_partial_scan3')  ORDER BY 1,2,3,4,5;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan3_i_idx', 2), 'ao_partial_scan3_i_idx');

-- Now desummarize the range with 1 block of aborted rows and 2 blocks of
-- committed rows.
1U: SELECT brin_desummarize_range('ao_partial_scan3_i_idx', 33554432);

-- Summarizing this range should scan blocks corresponding to the 2 final logical
-- heap blocks in the range only. This means that we will restrict our scan to
-- the varblocks specified by the varblock entries:
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('ao_partial_scan3')
    WHERE first_row_no < ((33554435 - 33554432) * 32768) ORDER BY 1,2,3,4,5;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

SELECT brin_summarize_range('ao_partial_scan3_i_idx', 33554432);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan3_i_idx', 2), 'ao_partial_scan3_i_idx');

--------------------------------------------------------------------------------
-- Scenario 4: Starting block number of scan maps to hole between two entries
-- in a minipage.
--------------------------------------------------------------------------------

CREATE TABLE ao_partial_scan4(i int, j int) USING ao_row;

-- Insert one logical heap block worth of committed rows.
INSERT INTO ao_partial_scan4 SELECT 1, j FROM generate_series(1, 32767) j;
-- Insert one more row for the next logical heap block.
INSERT INTO ao_partial_scan4 SELECT 20, j FROM generate_series(1, 1) j;

-- Doing an index build will result in scanning the whole relation.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON ao_partial_scan4 USING brin(i) WITH (pages_per_range = 1);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- We have 20 block directory entries, with the first 19 covering the first
-- logical heap block and the last one covering the second logical heap block.
-- Note that there is a hole between the last 2 entries.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('ao_partial_scan4') ORDER BY 1,2,3,4,5;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan4_i_idx', 2), 'ao_partial_scan4_i_idx');

-- Now desummarize a range.
1U: SELECT brin_desummarize_range('ao_partial_scan4_i_idx', 33554433);

-- Summarizing 33554433 will map to a hole between block directory entries, so
-- we will start our scan from the entry preceding the hole. So, we will scan
-- the last two varblocks.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('ao_partial_scan4_i_idx', 33554433);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('ao_partial_scan4_i_idx', 2), 'ao_partial_scan4_i_idx');

--------------------------------------------------------------------------------
----                          ao_column tables
--------------------------------------------------------------------------------

-- In the test below we will make a column in the brin index contain values 
-- that are equal to their respective row numbers. With that setup, we can check
-- if the brin range start row matches the block start row of the given column.
CREATE OR REPLACE FUNCTION verify_rownum_vs_values(indexname text, colno int)
RETURNS TABLE(blknum bigint, match boolean) AS
$$
BEGIN /* in func */
    RETURN QUERY EXECUTE format( /* in func */
           'SELECT p.blknum, (string_to_array(trim(both ''{}'' from p.value), '' ''))[1] = t.min::text' /* in func */
        || ' FROM brin_page_items(get_raw_page(%L, 2), %L) p' /* in func */
        || ' JOIN (SELECT right(split_part(ctid::text, '','', 1), -1) AS blknum,min(i)' /* in func */
        || '       FROM aoco_partial_scan1 GROUP BY 1) t ON t.blknum = p.blknum::text' /* in func */
        || '       WHERE p.attnum = %s', indexname, indexname, colno); /* in func */
END; /* in func */
$$
LANGUAGE plpgsql;

CREATE TABLE aoco_partial_scan1(d int, i int, j int2) USING ao_column DISTRIBUTED BY (d);

--------------------------------------------------------------------------------
-- Scenario 1: Starting block number of scans map to block directory entries,
-- across multiple minipages, corresponding to multiple segfiles.
--------------------------------------------------------------------------------

-- Create a couple of seg files, spanning a couple of minipages each.
-- We would want vary the values for each columns and then verify the summarization later.
-- Note that column j is int2 so we just vary within the same block.
1: BEGIN;
2: BEGIN;
1: INSERT INTO aoco_partial_scan1 SELECT 1,k,k/32768 FROM generate_series(1, 1320000) k;
2: INSERT INTO aoco_partial_scan1 SELECT 20,k,k/32768 FROM generate_series(1, 1320000) k;
1: COMMIT;
2: COMMIT;

-- Doing an index build will result in scanning the relation whole.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON aoco_partial_scan1 USING brin(i, j) WITH (pages_per_range = 3);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1U: SELECT tupleid, columngroup_no, segno, count(entry_no) AS num_entries, sum(row_count) AS total_rowcount
FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan1')
WHERE columngroup_no IN (1, 2) GROUP BY tupleid, columngroup_no, segno
ORDER BY 1,2,3;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan1_i_j_idx', 2), 'aoco_partial_scan1_i_j_idx');
1U: SELECT * FROM verify_rownum_vs_values('aoco_partial_scan1_i_j_idx', 1);

-- Now desummarize a few ranges.
1U: SELECT brin_desummarize_range('aoco_partial_scan1_i_j_idx', 33554432);
1U: SELECT brin_desummarize_range('aoco_partial_scan1_i_j_idx', 33554438);
1U: SELECT brin_desummarize_range('aoco_partial_scan1_i_j_idx', 33554471);
1U: SELECT brin_desummarize_range('aoco_partial_scan1_i_j_idx', 67108867);

-- Range scans beginning at 33554432 and 33554438 will span 13 block directory
-- entries (13 varblocks) for col i and 7 entries (7 varblocks) for col j.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan1')
  WHERE first_row_no < ((33554435 - 33554432) * 32768) AND columngroup_no IN (1, 2)
  AND segno = 1 ORDER BY 1,2,3,4,5;

-- Now summarize these desummarized ranges piecemeal and check that we scan only
-- a subset of the blocks each time.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan1_i_j_idx', 33554432);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan1_i_j_idx', 2), 'aoco_partial_scan1_i_j_idx');
1U: SELECT * FROM verify_rownum_vs_values('aoco_partial_scan1_i_j_idx', 1);

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan1_i_j_idx', 33554438);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan1_i_j_idx', 2), 'aoco_partial_scan1_i_j_idx');
1U: SELECT * FROM verify_rownum_vs_values('aoco_partial_scan1_i_j_idx', 1);

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan1_i_j_idx', 33554471);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan1_i_j_idx', 2), 'aoco_partial_scan1_i_j_idx');
1U: SELECT * FROM verify_rownum_vs_values('aoco_partial_scan1_i_j_idx', 1);

-- A similar result is expected from scanning the first range in seg2, as for
-- the first range in seg1.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan1_i_j_idx', 67108867);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan1_i_j_idx', 2), 'aoco_partial_scan1_i_j_idx');
1U: SELECT * FROM verify_rownum_vs_values('aoco_partial_scan1_i_j_idx', 1);

--------------------------------------------------------------------------------
-- Scenario 2: Starting block number of scan maps to hole at the end of the
-- minipage (after the last entry).
--------------------------------------------------------------------------------
CREATE TABLE aoco_partial_scan2(d int, i int, j int) USING ao_column DISTRIBUTED BY (d);
-- Fill 1 logical heap block with committed rows.
INSERT INTO aoco_partial_scan2 SELECT 1, k, k FROM generate_series(1, 32767) k;
-- Now add some aborted rows at the end of the segfile, resulting in a hole at
-- the end of the minipage.
BEGIN;
INSERT INTO aoco_partial_scan2 SELECT 20, k, 200 FROM generate_series(1, 32768) k;
ABORT;

-- Doing an index build will result in scanning the committed rows only.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON aoco_partial_scan2 USING brin(i, j) WITH (pages_per_range = 1);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Per colgroup, we have 1 minipage with 5 entries, meaning that there have 10
-- varblocks for colno=0,1.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan2')
  WHERE columngroup_no IN (1, 2) ORDER BY 1,2,3,4,5;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan2_i_j_idx', 2), 'aoco_partial_scan2_i_j_idx');
1U: SELECT * FROM verify_rownum_vs_values('aoco_partial_scan2_i_j_idx', 1);

-- Now desummarize the first range of committed rows.
1U: SELECT brin_desummarize_range('aoco_partial_scan2_i_j_idx', 33554432);

-- Summarizing the first range now should only scan the committed rows. (same
-- as the index build). In other words, we will scan an equal number of
-- varblocks as there are block directory entries, for col = (i, j) starting
-- from entry_no = 0.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan2_i_j_idx', 33554432);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- The start of 33554433 falls into a hole at the end of the segfile. In this
-- case we will start our scan from the last block directory entry and will
-- read that one varblock each for col = (i, j).
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan2_i_j_idx', 33554433);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan2_i_j_idx', 2), 'aoco_partial_scan2_i_j_idx');
1U: SELECT * FROM verify_rownum_vs_values('aoco_partial_scan2_i_j_idx', 1);

--------------------------------------------------------------------------------
-- Scenario 3: Starting block number of scan maps to hole at the start of the
-- segfile (and before the first entry of the first minipage).
--------------------------------------------------------------------------------
CREATE TABLE aoco_partial_scan3(d int, i int, j int) USING ao_column DISTRIBUTED BY (d);
-- Create a hole with 1 logical heap block worth of aborted rows.
BEGIN;
INSERT INTO aoco_partial_scan3 SELECT 1, k, k FROM generate_series(1, 32767) k;
ABORT;
-- Fill the next 3 logical heap blocks with committed rows.
INSERT INTO aoco_partial_scan3 SELECT 20, k, k/32768 FROM generate_series(1, 32768 * 3) k;

-- Doing an index build will result in scanning the committed rows only.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON aoco_partial_scan3 USING brin(i, j) WITH (pages_per_range = 3);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- We have 1 minipage with 13 entries, meaning that there are 13 varblocks worth
-- of committed rows, for each col. The first entry's firstRowNum reveals the
-- hole at the beginning.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan3')
    WHERE columngroup_no IN (1, 2) ORDER BY 1,2,3,4,5;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan3_i_j_idx', 2), 'aoco_partial_scan3_i_j_idx');

-- Now desummarize the range with 1 block of aborted rows and 2 blocks of
-- committed rows.
1U: SELECT brin_desummarize_range('aoco_partial_scan3_i_j_idx', 33554432);

-- Summarizing this range should scan blocks corresponding to the 2 final logical
-- heap blocks in the range only. This means that we will restrict our scan to
-- the varblocks specified by the varblock entries:
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan3')
    WHERE columngroup_no IN (1, 2) AND
          first_row_no < ((33554435 - 33554432) * 32768) ORDER BY 1,2,3,4,5;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan3_i_j_idx', 33554432);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan3_i_j_idx', 2), 'aoco_partial_scan3_i_j_idx');

--------------------------------------------------------------------------------
-- Scenario 4: Starting block number of scan maps to hole between two entries
-- in a minipage.
--------------------------------------------------------------------------------

CREATE TABLE aoco_partial_scan4(i int) USING ao_column;

-- Insert one logical heap block worth of committed rows.
INSERT INTO aoco_partial_scan4 SELECT 1 FROM generate_series(1, 32767);
-- Insert one more row for the next logical heap block.
INSERT INTO aoco_partial_scan4 SELECT 20 FROM generate_series(1, 1);

-- Doing an index build will result in scanning the whole relation.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON aoco_partial_scan4 USING brin(i) WITH (pages_per_range = 1);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- We have 6 block directory entries, with the first 5 covering the first
-- logical heap block and the last one covering the second logical heap block.
-- Note that there is a hole between the last 2 entries.
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan4') ORDER BY 1,2,3,4,5;

-- Show the composition of the single data page in the BRIN index.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan4_i_idx', 2), 'aoco_partial_scan4_i_idx');

-- Now desummarize a range.
1U: SELECT brin_desummarize_range('aoco_partial_scan4_i_idx', 33554433);

-- Summarizing 33554433 will map to a hole between block directory entries, so
-- we will start our scan from the entry preceding the hole. So, we will scan
-- the last two varblocks
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT brin_summarize_range('aoco_partial_scan4_i_idx', 33554433);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- Sanity: the summary info is reflected in the data page.
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan4_i_idx', 2), 'aoco_partial_scan4_i_idx');

--------------------------------------------------------------------------------
-- Scenario 5: BRIN index includes a column that has missing rownums
--------------------------------------------------------------------------------

CREATE TABLE aoco_partial_scan5(i int) USING ao_column;

-- Insert one logical heap block worth of committed rows.
INSERT INTO aoco_partial_scan5 SELECT 1 FROM generate_series(1, 32767);

-- add a new column j
ALTER TABLE aoco_partial_scan5 ADD COLUMN j int DEFAULT 10;

-- First index: built on column i and j.
-- Doing an index build will result in scanning the table.
-- But the new column won't have any blocks to scan.
-- The number of scanned blocks equal to the total number of blocks of the
-- table shown by gp_toolkit.__gp_aoblkdir later.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON aoco_partial_scan5 USING brin(i, j) WITH (pages_per_range = 3);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- no blkdir entry for column j
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan5') ORDER BY 1,2,3,4,5;

-- but the summarization shows expected default value for the column j
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan5_i_j_idx', 2), 'aoco_partial_scan5_i_j_idx');

-- Second index built only on column j. The block count should be the same as 
-- the first index because we'll use the first column as the anchor column.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'skip', '', '', '', 1, -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
CREATE INDEX ON aoco_partial_scan5 USING brin(j) WITH (pages_per_range = 3);
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'status', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- we should see correct summarization even the only column is incomplete
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan5_j_idx', 2), 'aoco_partial_scan5_j_idx');

-- now desummarize everything
SELECT brin_desummarize_range('aoco_partial_scan5_i_j_idx', 33554432);
SELECT brin_desummarize_range('aoco_partial_scan5_j_idx', 33554432);

-- Insert some new rows. The new column will physically contain values for these rows.
INSERT INTO aoco_partial_scan5 SELECT 20, k FROM generate_series(1, 50000) k;

-- column j will have blkdir entries for the newly added rows
1U: SELECT * FROM gp_toolkit.__gp_aoblkdir('aoco_partial_scan5') ORDER BY 1,2,3,4,5;

-- summarize from the very first rownum which is missing in column j
SELECT brin_summarize_range('aoco_partial_scan5_i_j_idx', 33554432);
SELECT brin_summarize_range('aoco_partial_scan5_j_idx', 33554432);

-- but we should see correct summarization for it
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan5_i_j_idx', 2), 'aoco_partial_scan5_i_j_idx');
1U: SELECT * FROM brin_page_items(get_raw_page('aoco_partial_scan5_j_idx', 2), 'aoco_partial_scan5_j_idx');

