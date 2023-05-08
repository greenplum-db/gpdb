-- Test COPY progress summary view

-- setup replicated table and data files for COPY
CREATE TABLE t_copy_repl (a INT, b INT) DISTRIBUTED REPLICATED;
INSERT INTO t_copy_repl VALUES (2, 2), (0, 0), (5, 5);
COPY t_copy_repl TO '/tmp/t_copy_relp<SEGID>' ON SEGMENT;
-- setup DISTRIBUTED table and data files for COPY
CREATE TABLE t_copy_d (a INT, b INT);
INSERT INTO t_copy_d select i, i from generate_series(1, 12) i;
COPY t_copy_d TO '/tmp/t_copy_d<SEGID>' ON SEGMENT;

-- Suspend copy after processed 2 tuples on each segment (3 segments)
select gp_inject_fault_infinite('copy_processed_two_tuples', 'suspend', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- It is intentional to run same query twice in 2 sessions. The progress summary
-- view should record the progress of each session in separate rows.

-- session 1 and 2: Replicated table COPY TO FILE ON SEGMENT
1&: COPY t_copy_repl TO '/tmp/t_copy_to_relp<SEGID>' ON SEGMENT;
2&: COPY t_copy_repl TO '/tmp/t_copy_to_relp<SEGID>' ON SEGMENT;
-- session 3 and 4: Replicated table COPY FROM STDIN
3&: COPY t_copy_repl FROM PROGRAM 'for i in `seq 1 3`; do echo $i $i; done' WITH DELIMITER ' ';
4&: COPY t_copy_repl FROM PROGRAM 'for i in `seq 1 3`; do echo $i $i; done' WITH DELIMITER ' ';
-- session 5 & 6: Replicated table COPY FROM FILE ON SEGMENT
5&: COPY t_copy_repl FROM '/tmp/t_copy_relp<SEGID>' ON SEGMENT;
6&: COPY t_copy_repl FROM '/tmp/t_copy_relp<SEGID>' ON SEGMENT;
-- session 7 & 8: Distributed table COPY TO STDOUT
7&: COPY t_copy_d TO STDOUT;
8&: COPY t_copy_d TO STDOUT;
-- session 9 & 10: Distributed table COPY TO FILE ON SEGMENT
9&: COPY t_copy_d TO '/tmp/t_copy_to_d<SEGID>' ON SEGMENT;
10&: COPY t_copy_d TO '/tmp/t_copy_to_d<SEGID>' ON SEGMENT;
-- session 11 & 12: Distributed table COPY FROM PROGRAM
11&: COPY t_copy_d FROM PROGRAM 'for i in `seq 1 12`; do echo $i $i; done' WITH DELIMITER ' ';
12&: COPY t_copy_d FROM PROGRAM 'for i in `seq 1 12`; do echo $i $i; done' WITH DELIMITER ' ';
-- session 13 & 14: Distributed table COPY FROM FILE ON SEGMENT
13&: COPY t_copy_d FROM '/tmp/t_copy_d<SEGID>' ON SEGMENT;
14&: COPY t_copy_d FROM '/tmp/t_copy_d<SEGID>' ON SEGMENT;

SELECT gp_wait_until_triggered_fault('copy_processed_two_tuples', 1, dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';

-- Verify the progress views
SELECT
    gspc.gp_segment_id,
    gspc.datname,
    gspc.relid::regclass,
    gspc.command,
    gspc."type",
    gspc.bytes_processed,
    gspc.bytes_total,
    gspc.tuples_processed,
    gspc.tuples_excluded
FROM gp_stat_progress_copy gspc
    JOIN gp_stat_activity ac USING (pid)
ORDER BY (ac.sess_id, gspc.gp_segment_id);

SELECT
    ac.gp_segment_id = -1 as has_coordinator_pid,
    gspcs.datname,
    gspcs.relid::regclass,
    gspcs.command,
    gspcs."type",
    gspcs.bytes_processed,
    gspcs.bytes_total,
    gspcs.tuples_processed,
    gspcs.tuples_excluded
FROM gp_stat_progress_copy_summary gspcs
    JOIN gp_stat_activity ac USING (pid)
ORDER BY ac.sess_id;

SELECT gp_inject_fault('copy_processed_two_tuples', 'reset', dbid) FROM gp_segment_configuration WHERE content > -1 AND role = 'p';
1<:
2<:
3<:
4<:
5<:
6<:
7<:
8<:
9<:
10<:
11<:
12<:
13<:
14<:

-- Test COPY TO STDOUT
-- We need to run this test separately because the COPY TO with replicated table
-- only copies data from on segment 0.

-- Suspend copy after processing 2 tuples on segment 0 only
select gp_inject_fault_infinite('copy_processed_two_tuples', 'suspend', dbid) FROM gp_segment_configuration WHERE content = 0 AND role = 'p';

-- session 1: copy table to pipe
1&: COPY t_copy_repl TO STDOUT;
-- session 2: copy same table to pipe
2&: COPY t_copy_repl TO STDOUT;
SELECT gp_wait_until_triggered_fault('copy_processed_two_tuples', 1, dbid) FROM gp_segment_configuration WHERE content = 0 AND role = 'p';

-- session 2: query gp_stat_progress_copy while the copy is running, the view should indicate 2 tuples have been processed for segment 0 only
SELECT gp_segment_id, datname, relid::regclass, command, "type", bytes_processed, bytes_total, tuples_processed, tuples_excluded FROM gp_stat_progress_copy;
SELECT pid IS NOT NULL as has_pid, datname, relid::regclass, command, "type", bytes_processed, bytes_total, tuples_processed, tuples_excluded FROM gp_stat_progress_copy_summary;

-- Reset fault injector
SELECT gp_inject_fault('copy_processed_two_tuples', 'reset', dbid) FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
1<:
2<:

-- teardown
DROP TABLE t_copy_repl;
DROP TABLE t_copy_d;
