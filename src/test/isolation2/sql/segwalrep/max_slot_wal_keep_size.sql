-- when the WAL replication lag exceeds 'max_slot_wal_keep_size', the extra WAL
-- log will be removed on the primary and the replication slot will be marked as
-- obsoleted. In this case, the mirror will be marked down as well and need full
-- recovery to brought it back. 

include: helpers/server_helpers.sql;

CREATE OR REPLACE FUNCTION advance_xlog(num int) RETURNS void AS
$$
DECLARE
	i int; 
BEGIN 
    i := 0; 
	CREATE TABLE t_dummy_switch(i int) DISTRIBUTED BY (i); 
	LOOP 
		IF i >= num THEN 
			DROP TABLE t_dummy_switch; 
			RETURN; 
		END IF; 
		PERFORM pg_switch_xlog() FROM gp_dist_random('gp_id') WHERE gp_segment_id=0; 
		INSERT INTO t_dummy_switch SELECT generate_series(1,10); 
		i := i + 1; 
	END LOOP; 
	DROP TABLE t_dummy_switch; 
END; 
$$ language plpgsql;

-- On content 0 primary, retain max 64MB (1 WAL file) for replication
-- slots.  The other GUCs are needed to make the test run faster.
0U: ALTER SYSTEM SET max_slot_wal_keep_size TO 64;
0U: ALTER SYSTEM SET wal_keep_segments TO 0;
0U: ALTER SYSTEM SET gp_fts_mark_mirror_down_grace_period TO 0;
0U: select pg_reload_conf();
-- And on coordinator, also to make the test faster.
ALTER SYSTEM SET gp_fts_probe_retries TO 1;
select pg_reload_conf();

-- Create a checkpoint now so that another checkpoint doesn't get
-- triggered when the test doesn't expect it to.
CHECKPOINT;

-- walsender skip sending WAL to the mirror
1: SELECT gp_inject_fault_infinite('walsnd_skip_send', 'skip', dbid) FROM gp_segment_configuration WHERE content=0 AND role='p';

2: BEGIN;
2: DROP TABLE IF EXISTS t_slot_size_limit;
2: CREATE TABLE t_slot_size_limit(a int);
2: INSERT INTO t_slot_size_limit SELECT generate_series(1,1000);

-- generate 2 more WAL files, which exceeds 'max_slot_wal_keep_size'
2: SELECT advance_xlog(2);

-- checkpoint will trigger the check of obsolete replication slot, it will stop the walsender.
0U: CHECKPOINT;

-- Count of WAL files in pg_xlog should not exceed XLOGfileslop + 1,
-- where 1 is the max_slot_wal_keep_size set above.
0U: select count(pg_ls_dir) < current_setting('checkpoint_segments')::int * 2 + 2
    from pg_ls_dir('pg_xlog') where pg_ls_dir like '________________________';

-- Replication slot on content 0 primary should report invalid LSN
-- because the WAL file it needs is already removed when checkpoint
-- was created.
0U: select * from pg_get_replication_slots();
0Uq:

1: SELECT gp_inject_fault_infinite('walsnd_skip_send', 'reset', dbid) FROM gp_segment_configuration WHERE content=0 AND role='p';
1: SELECT gp_request_fts_probe_scan();
2: END;

-- check the mirror is down and the sync_error is set.
1: SELECT role, preferred_role, status FROM gp_segment_configuration WHERE content = 0;
1: SELECT sync_error FROM gp_stat_replication WHERE gp_segment_id = 0;

-- do full recovery
!\retcode gprecoverseg -aF;
select wait_until_segment_synchronized(0);

-- the mirror is up and the replication is back
1: SELECT role, preferred_role, status FROM gp_segment_configuration WHERE content = 0;
1: SELECT state, sync_error FROM gp_stat_replication WHERE gp_segment_id = 0;

0U: ALTER SYSTEM RESET max_slot_wal_keep_size;
0U: ALTER SYSTEM RESET wal_keep_segments;
0U: ALTER SYSTEM RESET gp_fts_mark_mirror_down_grace_period;
0U: select pg_reload_conf();
0Uq:
ALTER SYSTEM RESET gp_fts_probe_retries;
select pg_reload_conf();

1q:
2q:
