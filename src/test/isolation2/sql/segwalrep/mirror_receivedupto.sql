-- Test the scenario where mirror startup process doesn't hang trying
-- to replay wrong xlog records.
--
-- Highlevel sequence of events test is validating (with made up
-- numbers for explanation):
-- 1. Primary writes record at LSN 16k of size 32k, effectively moves
-- the LSN to 48k. The record is split across page1 (16k) and page2
-- (16k). The transaction is yet to be committed, hence no flush
-- happens. The big record in test is created using the appendonly
-- table of roughly 48K.
--
-- 2. Background flush kicks in and since flushes only full pages of
-- xlog, flushes page1 means only till LSN 32k and not 48k. Also, as a
-- result, streams it to mirror and mirror updates its receivedUpto
-- 32k. Checking for wal writer performed background flush is
-- validated using fault injector 'in_xlog_background_flush'.
--
-- 3. Primary crashes and performs recovery. Since the last record is
-- not valid (only half present) considers LSN 16k as the new
-- insertion point again. Crash is invoked using the panic fault at
-- 'start_prepare'.
--
-- 4. Primary writes a new record at LSN 16k of size 1k only this time
-- and commits. So, xlog gets flushed till LSN 17k. Writing very small
-- xlog record after recovery is achived in test using very small
-- insert to a heap table.
--
-- 5. Since primary restarted, mirror disconnects. On reconnection
-- 'receivedUpto' (rollsback) gets reinitialized to start of streaming
-- point which is start of xlog segment for 17k. Hence gets new
-- records created on primary and discards old partial record.
--
-- 6. startup process replays new record written by primary instead of
-- attempting to replay partial AO record.

-- start_ignore
create language plpythonu;
-- end_ignore

CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
CREATE or REPLACE function wait_for_replay_match_flush_location() returns bool as
$$
declare /* in func */
        result bool; /* in func */
        retries int; /* in func */
begin /* in func */
        retries := 600; /* 2 minutes wait */
        loop /* in func */
                if (select replay_location = flush_location from gp_stat_replication where gp_segment_id=0) then /* in func */
                   return true; /* in func */
        end if; /* in func */
        perform pg_sleep(0.2); /* in func */
        retries := retries - 1; /* in func */
        if retries < 0 then /* in func */
                   return false; /* in func */
        end if; /* in func */
    end loop; /* in func */
end; /* in func */
$$ language plpgsql;

create or replace function pg_ctl_restart(datadir text)
returns text as $$
    import subprocess
    cmd = 'pg_ctl -l postmaster.log -D %s -w -m immediate restart' % datadir
    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;

CREATE TABLE table_for_big_record (a int, b int[]) WITH (appendonly=true, blocksize=2097152);
CREATE TABLE table_for_small_record (c int2);

-- to make sure no following select statement like 'select
-- wait_for_replay_match_flush_location()' generates xlog record.
VACUUM;

-- '0/0'::pg_lsn is used to avoid having xlog location in answer file
SELECT pg_switch_xlog() > '0/0'::pg_lsn FROM gp_dist_random('gp_id') WHERE gp_segment_id = 0;
CHECKPOINT;

-- block fts
SELECT gp_inject_fault_infinite('fts_probe', 'skip', 1);
SELECT gp_request_fts_probe_scan();
select gp_wait_until_triggered_fault('fts_probe', 1, 1);

-- panic and restart segment 1
SELECT gp_inject_fault('start_prepare', 'panic', dbid)
FROM gp_segment_configuration WHERE role = 'p' AND status = 'u' AND content = 0;
begin;
SELECT gp_inject_fault('in_xlog_background_flush', 'skip', dbid)
FROM gp_segment_configuration WHERE role = 'p' AND status = 'u' AND content = 0;
INSERT INTO table_for_big_record SELECT 4, array_agg(x) FROM generate_series(4, 12000) x;
select gp_wait_until_triggered_fault('in_xlog_background_flush', 1, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND status = 'u' AND content = 0;
commit;
1: INSERT INTO table_for_small_record VALUES (1), (2), (4);
-- check startup process is not stuck by comparing replay location and
-- flush location on mirror
SELECT wait_for_replay_match_flush_location();
-- start_ignore
-- just for debugging incase of failures
select * from gp_stat_replication where gp_segment_id=0;
-- end_ignore

-- restart segment 1 again
select pg_ctl_restart((select datadir from gp_segment_configuration c where c.role='p' and c.content=0));
-- step to validate walreceiver is spawned and records are flowing to
-- mirror after above restart of primary. If startup process is stuck
-- on mirror due to bug being verified, it missed starting
-- walreceiver. This used to cause the below transaction to get stuck
-- as synchoronous replication is used.
--
-- use new connection to make sure primary completely restarted and
-- accepting connections
2: INSERT INTO table_for_small_record VALUES (4);
2: SELECT gp_inject_fault('fts_probe', 'reset', 1);
