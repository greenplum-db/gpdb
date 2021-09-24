-- Setup fault injectors.
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- Skip FTS probes for this test to avoid segment being marked down on restart.
1:SELECT gp_inject_fault_infinite('fts_probe', 'skip', dbid)
    FROM gp_segment_configuration WHERE role='p' AND content=-1;
1:SELECT gp_request_fts_probe_scan();
1:SELECT gp_wait_until_triggered_fault('fts_probe', 1, dbid)
    FROM gp_segment_configuration WHERE role='p' AND content=-1;

CREATE TABLE bm_update_words_backup_block (id int) WITH (appendonly = true);

1: BEGIN;
2: BEGIN;
1: INSERT INTO bm_update_words_backup_block SELECT i%100 FROM generate_series(1, 200) AS i;
2: INSERT INTO bm_update_words_backup_block SELECT i%100 FROM generate_series(1, 200) AS i;
1: COMMIT;
2: COMMIT;

CREATE INDEX bm_update_words_backup_block_idx ON bm_update_words_backup_block USING bitmap (id);

-- INSERTs will attempt to add a bitmap page but will cause a word
-- expansion and a bitmap page split due to overflow. See bitmap
-- function updatesetbit_inpage().
2: INSERT INTO bm_update_words_backup_block VALUES (97);
2: INSERT INTO bm_update_words_backup_block VALUES (97), (99);

-- Run a CHECKPOINT to force this next INSERT to add backup blocks of
-- the two bitmap pages to its XLOG_BITMAP_UPDATEWORDS record.
2: CHECKPOINT;
2: INSERT INTO bm_update_words_backup_block VALUES (97);

-- Do an immediate restart to force crash recovery. The above INSERT
-- should be replayed with the backup blocks.
1: SELECT pg_ctl(datadir, 'restart') FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
3: INSERT INTO bm_update_words_backup_block VALUES (97);

-- Turn FTS back on.
3:SELECT gp_inject_fault('fts_probe', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content=-1;
