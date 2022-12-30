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
