--
-- Test Statistics Collector
--
-- Must be run after tenk2 has been created (by create_table),
-- populated (by create_misc) and indexed (by create_index).
--

-- conditio sine qua non
SHOW track_counts;  -- must be on

-- ensure that both seqscan and indexscan plans are allowed
SET enable_seqscan TO on;
SET enable_indexscan TO on;
-- for the moment, we don't want index-only scans here
SET enable_indexonlyscan TO off;

-- wait to let any prior tests finish dumping out stats;
-- else our messages might get lost due to contention
SELECT pg_sleep_for('2 seconds');

-- save counters
CREATE TEMP TABLE prevstats AS
SELECT t.seq_scan, t.seq_tup_read, t.idx_scan, t.idx_tup_fetch,
       (b.heap_blks_read + b.heap_blks_hit) AS heap_blks,
       (b.idx_blks_read + b.idx_blks_hit) AS idx_blks,
       pg_stat_get_snapshot_timestamp() as snap_ts
  FROM pg_catalog.pg_stat_user_tables AS t,
       pg_catalog.pg_statio_user_tables AS b
 WHERE t.relname='tenk2' AND b.relname='tenk2';

-- function to wait for counters to advance
create function wait_for_stats() returns void as $$
declare
  start_time timestamptz := clock_timestamp();
  updated bool;
begin
  -- we don't want to wait forever; loop will exit after 30 seconds
  for i in 1 .. 300 loop

    -- check to see if indexscan has been sensed
    SELECT (st.idx_scan >= pr.idx_scan + 1) INTO updated
      FROM pg_stat_user_tables AS st, pg_class AS cl, prevstats AS pr
     WHERE st.relname='tenk2' AND cl.relname='tenk2';

    exit when updated;

    -- wait a little
    perform pg_sleep(0.1);

    -- reset stats snapshot so we can test again
    perform pg_stat_clear_snapshot();

  end loop;

  -- report time waited in postmaster log (where it won't change test output)
  raise log 'wait_for_stats delayed % seconds',
    extract(epoch from clock_timestamp() - start_time);
end
$$ language plpgsql;

-- test effects of TRUNCATE on n_live_tup/n_dead_tup counters
CREATE TABLE trunc_stats_test(id serial);
CREATE TABLE trunc_stats_test1(id serial);
CREATE TABLE trunc_stats_test2(id serial);
CREATE TABLE trunc_stats_test3(id serial);
CREATE TABLE trunc_stats_test4(id serial);

-- check that n_live_tup is reset to 0 after truncate
INSERT INTO trunc_stats_test DEFAULT VALUES;
INSERT INTO trunc_stats_test DEFAULT VALUES;
INSERT INTO trunc_stats_test DEFAULT VALUES;
TRUNCATE trunc_stats_test;

-- test involving a truncate in a transaction; 4 ins but only 1 live
INSERT INTO trunc_stats_test1 DEFAULT VALUES;
INSERT INTO trunc_stats_test1 DEFAULT VALUES;
INSERT INTO trunc_stats_test1 DEFAULT VALUES;
UPDATE trunc_stats_test1 SET id = id + 10 WHERE id IN (1, 2);
DELETE FROM trunc_stats_test1 WHERE id = 3;

BEGIN;
UPDATE trunc_stats_test1 SET id = id + 100;
TRUNCATE trunc_stats_test1;
INSERT INTO trunc_stats_test1 DEFAULT VALUES;
COMMIT;

-- use a savepoint: 1 insert, 1 live
BEGIN;
INSERT INTO trunc_stats_test2 DEFAULT VALUES;
INSERT INTO trunc_stats_test2 DEFAULT VALUES;
SAVEPOINT p1;
INSERT INTO trunc_stats_test2 DEFAULT VALUES;
TRUNCATE trunc_stats_test2;
INSERT INTO trunc_stats_test2 DEFAULT VALUES;
RELEASE SAVEPOINT p1;
COMMIT;

-- rollback a savepoint: this should count 4 inserts and have 2
-- live tuples after commit (and 2 dead ones due to aborted subxact)
BEGIN;
INSERT INTO trunc_stats_test3 DEFAULT VALUES;
INSERT INTO trunc_stats_test3 DEFAULT VALUES;
SAVEPOINT p1;
INSERT INTO trunc_stats_test3 DEFAULT VALUES;
INSERT INTO trunc_stats_test3 DEFAULT VALUES;
TRUNCATE trunc_stats_test3;
INSERT INTO trunc_stats_test3 DEFAULT VALUES;
ROLLBACK TO SAVEPOINT p1;
COMMIT;

-- rollback a truncate: this should count 2 inserts and produce 2 dead tuples
BEGIN;
INSERT INTO trunc_stats_test4 DEFAULT VALUES;
INSERT INTO trunc_stats_test4 DEFAULT VALUES;
TRUNCATE trunc_stats_test4;
INSERT INTO trunc_stats_test4 DEFAULT VALUES;
ROLLBACK;

-- do a seqscan
SELECT count(*) FROM tenk2;
-- do an indexscan
SELECT count(*) FROM tenk2 WHERE unique1 = 1;

-- force the rate-limiting logic in pgstat_report_tabstat() to time out
-- and send a message
SELECT pg_sleep(1.0);

-- wait for stats collector to update
SELECT wait_for_stats();

-- check effects
SELECT relname, n_tup_ins, n_tup_upd, n_tup_del, n_live_tup, n_dead_tup
  FROM pg_stat_user_tables
 WHERE relname like 'trunc_stats_test%' order by relname;

SELECT st.seq_scan >= pr.seq_scan + 1,
       st.seq_tup_read >= pr.seq_tup_read + cl.reltuples,
       st.idx_scan >= pr.idx_scan + 1,
       st.idx_tup_fetch >= pr.idx_tup_fetch + 1
  FROM pg_stat_user_tables AS st, pg_class AS cl, prevstats AS pr
 WHERE st.relname='tenk2' AND cl.relname='tenk2';

SELECT st.heap_blks_read + st.heap_blks_hit >= pr.heap_blks + cl.relpages,
       st.idx_blks_read + st.idx_blks_hit >= pr.idx_blks + 1
  FROM pg_statio_user_tables AS st, pg_class AS cl, prevstats AS pr
 WHERE st.relname='tenk2' AND cl.relname='tenk2';

SELECT pr.snap_ts < pg_stat_get_snapshot_timestamp() as snapshot_newer
FROM prevstats AS pr;

DROP TABLE trunc_stats_test, trunc_stats_test1, trunc_stats_test2, trunc_stats_test3, trunc_stats_test4;
-- End of Stats Test
