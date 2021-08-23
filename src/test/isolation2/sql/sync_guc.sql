-- TEST 1: Fix Github issue https://github.com/greenplum-db/gpdb/issues/9208
1: create schema sync_np1;
1: create schema sync_np2;
1: CREATE OR REPLACE FUNCTION public.segment_setting(guc text)
    RETURNS SETOF text EXECUTE ON ALL SEGMENTS AS $$
    BEGIN RETURN NEXT pg_catalog.current_setting(guc); END
    $$ LANGUAGE plpgsql;
1q:
-- The SET command will create a Gang on the primaries, and the GUC
-- values should be the same on all QD/QEs.
2: show search_path;
2: set search_path = 'sync_np1,sync_np2';

-- The `reset_val` of `search_path` should be synchronized from the QD,
-- so, the GUC value will be also synchronized with the QD after RESET.
-- If the search_path is inconsistent between the QD and QEs after RESET,
-- creating the function will fail.
2: reset search_path;
2: select public.segment_setting('search_path');
2: create or replace function sync_f1() returns int as $$ select 1234; $$language sql;
2: select sync_f1();
2: drop function sync_f1();
2: drop schema sync_np1;
2: drop schema sync_np2;
2q:

-- TEST 2: Fix Github issue https://github.com/greenplum-db/gpdb/issues/685
-- `gp_select_invisible` is default to false. SET command will dispatch
-- the GUC's `reset_val` and changed value to the created Gang. If the QE(s)
-- use the incorrect `reset_val`, its value will be inconsistent with the QD's,
-- i.e. the `gp_select_invisible` is still false on the QEs.
3: show gp_select_invisible;
3: set gp_select_invisible = on;
3: reset gp_select_invisible;
3: select public.segment_setting('gp_select_invisible');
3: create table sync_t1(i int);
3: insert into sync_t1 select i from generate_series(1,10)i;
3: delete from sync_t1;
3: select * from sync_t1;
3: drop table sync_t1;
3q:

1: drop function public.segment_setting(guc text);
1q:

-- TEST 3: make sure all QEs call RESET if there are more than 1 QE of the session
-- in the primary
4: create temp table sync_t11(a int, b int) distributed by(b);
4: create temp table sync_t12(a int, b int) distributed by(a);

-- The join will create 2 slices on each primary, and 1 entrydb on the coordinator.
-- So, every primary and the coordinator should trigger 2 SET/RESET
-- We'll test SET/RESET xxx will be called for all QEs in the current session.
4: select relname from sync_t11, sync_t12, pg_class;

4: select gp_inject_fault('set_variable_fault', 'skip', dbid)
     from gp_segment_configuration where role='p';
4: set statement_mem = '12MB';
4: select gp_wait_until_triggered_fault('set_variable_fault', 2, dbid)
     from gp_segment_configuration where role='p';
4: select gp_inject_fault('set_variable_fault', 'reset', dbid)
     from gp_segment_configuration where role='p';

4: select gp_inject_fault('reset_variable_fault', 'skip', dbid)
     from gp_segment_configuration where role='p';
4: reset statement_mem;
4: select gp_wait_until_triggered_fault('reset_variable_fault', 2, dbid)
     from gp_segment_configuration where role='p';
4: select gp_inject_fault('reset_variable_fault', 'reset', dbid)
     from gp_segment_configuration where role='p';
4q:

