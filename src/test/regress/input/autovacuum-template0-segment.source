-- Test to validate autovacuum takes care of anti-XID wraparounds of
-- 'template0'. Because of that, the age of template0 should not go
-- above autovacuum_freeze_max_age (we assume the default of 200
-- million here)

create or replace function test_consume_xids(int4) returns void
as '@abs_srcdir@/regress.so', 'test_consume_xids'
language C;

-- start_ignore
\! gpconfig -c debug_burn_xids -v on --skipvalidation
\! gpconfig -c autovacuum_naptime -v 5
\! gpstop -au
-- end_ignore

-- Suspend the autovacuum worker from vacuuming before
-- ShmemVariableCache->latestCompletedXid is expected to be updated
SELECT gp_inject_fault('auto_vac_worker_before_do_autovacuum', 'suspend', '', 'template0', '', 1, -1, 0, dbid) from gp_segment_configuration where content = 0 and role = 'p';

select test_consume_xids(100 * 1000000) from gp_dist_random('gp_id') where gp_segment_id = 0;
select test_consume_xids(100 * 1000000) from gp_dist_random('gp_id') where gp_segment_id = 0;
select test_consume_xids(10 * 1000000) from gp_dist_random('gp_id') where gp_segment_id = 0;

select gp_segment_id, age(datfrozenxid) < 200 * 1000000 from gp_dist_random('pg_database') where datname='template0';

-- start_ignore
select gp_segment_id, datfrozenxid from gp_dist_random('pg_database') where datname='template0';
-- end_ignore

-- Wait until autovacuum is triggered
SELECT gp_wait_until_triggered_fault('auto_vac_worker_before_do_autovacuum', 1, dbid) from gp_segment_configuration where content = 0 and role = 'p';

-- track that we've updated the row in pg_database for template0
SELECT gp_inject_fault('vacuum_update_dat_frozen_xid', 'skip', '', 'template0', '', 1, -1, 0, dbid) from gp_segment_configuration where content = 0 and role = 'p';

SELECT gp_inject_fault('auto_vac_worker_before_do_autovacuum', 'reset', dbid) from gp_segment_configuration where content = 0 and role = 'p';

-- wait until autovacuum worker updates pg_database
SELECT gp_wait_until_triggered_fault('vacuum_update_dat_frozen_xid', 1, dbid) from gp_segment_configuration where content = 0 and role = 'p';
SELECT gp_inject_fault('vacuum_update_dat_frozen_xid', 'reset', dbid) from gp_segment_configuration where content = 0 and role = 'p';

-- template0 should be young
-- We only wait for 1 worker above to update the datfrozenxid. Based
-- on age of database multiple workers can be invoked to perform the
-- job. Given that number is not deterministic, we need to have retry
-- logic to check for age dropped for the database or not.
DO $$
DECLARE young BOOLEAN;
BEGIN
     FOR i in 1..1500 loop
	      SELECT age(datfrozenxid) < 200 * 1000000
	      FROM gp_dist_random('pg_database') WHERE datname='template0' AND gp_segment_id = 0
	      into young;

	      IF young THEN
	         raise notice 'Template0 is young now';
		 EXIT;
	      END IF;
	      PERFORM pg_sleep(0.2);
     END LOOP;

     IF not young THEN
	raise notice 'Template0 is still old';
     END IF;
END$$;

-- start_ignore
select gp_segment_id, datfrozenxid from gp_dist_random('pg_database') where datname='template0';
-- end_ignore

-- start_ignore
\! gpconfig -r debug_burn_xids --skipvalidation
\! gpconfig -r autovacuum_naptime
\! gpstop -au
-- end_ignore

