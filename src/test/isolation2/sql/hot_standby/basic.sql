-- Tests for basic query dispatch on a hot standy.

-- hot standby must show on and the sync mode is remote_apply for the tests to make sense
-1S: show hot_standby;
-1S: show synchronous_commit;

-- will be checking if QD/QE info looks good
-1S: select id, type, content, port from gp_backend_info();

----------------------------------------------------------------
-- Test: basic query dispatch
----------------------------------------------------------------
create table hs_t1(a int);
create table hs_t2(a int);

-- standby should see the results for 2pc immediately.
insert into hs_t1 select * from generate_series(1,10);
-1S: select * from hs_t1;
-- standby won't see results for the last 1pc immediately because the standby QD
-- isn't aware of of it so its distributed snapshot doesn't include the 1pc, but
-- as long as another 2pc comes it will be able to see the previous 1pc. Wee 
-- tolerate this case in the mirrored cluster setup.
insert into hs_t2 values(1);
-1S: select * from hs_t2;
-- any following 2pc will make the 1pc visible
create temp table tt(a int);
-1S: select * from hs_t2;

-- we have three QEs launched on the mirror segments.
-- note that the first QE on a segment is still a "writer" because we
-- need it to manage locks, same as read-only queries on a primary QD.
-1S: select id, type, content, port from gp_backend_info();

-- should have parallel readers launched
-1S: select * from hs_t1 join (select * from hs_t2) hs_t2 on (hs_t1 = hs_t2);
-1S: select id, type, content, port from gp_backend_info();

-- now a singleton reader added too
-1S: select * from hs_t1 join (select oid::int from pg_class) hs_t2 on (hs_t1 = hs_t2);
-1S: select id, type, content, port from gp_backend_info();

-- un-committed result should not be seen by the standby
begin;
insert into hs_t1 select * from generate_series(11,20);

-- standby should only see 1...10
-1S: select * from hs_t1;

end;

-- standby should see 1...20 now
-1S: select * from hs_t1;

----------------------------------------------------------------
-- Test: other things that a hot standby can do.
--
-- More refer to regress test 'hs_standby_allowed'.
----------------------------------------------------------------
-- set/reset and show GUC
-1S: set optimizer = on;
-1S: show optimizer;
-1S: reset optimizer;
-- copy command
-1S: copy hs_t1 to '/tmp/hs_copyto.csv' csv null '';
-- query catalogs
-1S: select count(*) from pg_class where relname = 'hs_t1';
-1S: select dbid,content,role,preferred_role,mode,status from gp_segment_configuration where dbid = current_setting('gp_dbid')::integer;
-- checkpoint is allowed on standby but a restart point is created instead
-1S: checkpoint;

----------------------------------------------------------------
-- Test: things that can't be done on a hot standby:
-- no DML, DDL or anything that generates WAL.
--
-- More refer to regress test 'hs_standby_disallowed'.
----------------------------------------------------------------
-1S: insert into hs_t1 values(1);
-1S: delete from hs_t1;
-1S: update hs_t1 set a = 0;
-1S: create table hs_t2(a int);
-1S: create database hs_db;
-1S: vacuum hs_t1;

--
-- No hintbit WAL generation in SELECT.
--
create table hs_nohintbit(a int) distributed by (a);
insert into hs_nohintbit select generate_series (1, 10);
-- flush the data to disk
checkpoint;

-1S: set gp_disable_tuple_hints=off;
-- no WAL is being generated (otherwise an error would occur "cannot make new WAL entries during recovery")
-1S: SELECT count(*) FROM hs_nohintbit;

