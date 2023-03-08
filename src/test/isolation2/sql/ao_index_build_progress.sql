-- Test to ensure we correctly report progress in pg_stat_progress_create_index
-- for append-optimized tables

-- AO table
CREATE TABLE ao_index_build_progress(i int, j bigint) USING ao_row
    WITH (compresstype=zstd, compresslevel=2);

-- Insert all tuples to seg1.
INSERT INTO ao_index_build_progress SELECT 0, i FROM generate_series(1, 100000) i;

-- Suspend execution when some blocks have been read.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'suspend', '', '', '', 10, 10, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1&: CREATE INDEX ON ao_index_build_progress(i);

-- Wait until some AO varblocks have been read.
SELECT gp_wait_until_triggered_fault('AppendOnlyStorageRead_ReadNextBlock_success', 10, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- By now, we should have reported some blocks (of size 'block_size') as "done",
-- as well as a total number of blocks that matches the relation's on-disk size.
1U: SELECT command, phase,
        (pg_relation_size('ao_index_build_progress') +
         (current_setting('block_size')::int - 1)) / current_setting('block_size')::int
        AS blocks_total_actual,
        blocks_total AS blocks_total_reported,
        blocks_done AS blocks_done_reported
    FROM pg_stat_progress_create_index
    WHERE relid = 'ao_index_build_progress'::regclass;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1<:

-- AOCO table
CREATE TABLE aoco_index_build_progress(i int, j int ENCODING (compresstype=zstd, compresslevel=2))
    USING ao_column;

-- Insert all tuples to seg1.
INSERT INTO aoco_index_build_progress SELECT 0, i FROM generate_series(1, 100000) i;

-- Suspend execution when some blocks have been read.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'suspend', '', '', '', 5, 5, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1&: CREATE INDEX ON aoco_index_build_progress(i);

-- Wait until some AOCO varblocks have been read.
SELECT gp_wait_until_triggered_fault('AppendOnlyStorageRead_ReadNextBlock_success', 5, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- By now, we should have reported some blocks (of size 'block_size') as "done",
-- as well as a total number of blocks that matches the relation's on-disk size.
-- Note: all blocks for the relation have to be scanned as we are building an
-- index for the first time and a block directory has to be created.
1U: SELECT command, phase,
           (pg_relation_size('aoco_index_build_progress') +
            (current_setting('block_size')::int - 1)) / current_setting('block_size')::int AS blocks_total_actual,
            blocks_total AS blocks_total_reported,
           blocks_done AS blocks_done_reported
    FROM pg_stat_progress_create_index
    WHERE relid = 'aoco_index_build_progress'::regclass;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1<:

-- Repeat the test for another index build

-- Suspend execution when some blocks have been read.
SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'suspend', '', '', '', 5, 5, 0, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1&: CREATE INDEX ON aoco_index_build_progress(j);

-- Wait until some AOCO varblocks have been read.
SELECT gp_wait_until_triggered_fault('AppendOnlyStorageRead_ReadNextBlock_success', 5, dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

-- By now, we should have reported some blocks (of size 'block_size') as "done",
-- as well as a total number of blocks that matches the size of col j's segfile.
-- Note: since we already had a block directory prior to the index build on
-- column 'j', only column 'j' will be scanned.
1U: SELECT command, phase,
           ((pg_stat_file(pg_relation_filepath('aoco_index_build_progress') || '.' || 129)).size
                + (current_setting('block_size')::int - 1)) / current_setting('block_size')::int
                AS col_j_blocks,
           blocks_total AS blocks_total_reported,
           blocks_done AS blocks_done_reported
    FROM pg_stat_progress_create_index
    WHERE relid = 'aoco_index_build_progress'::regclass;

SELECT gp_inject_fault('AppendOnlyStorageRead_ReadNextBlock_success', 'reset', dbid)
    FROM gp_segment_configuration WHERE content = 1 AND role = 'p';

1<:
