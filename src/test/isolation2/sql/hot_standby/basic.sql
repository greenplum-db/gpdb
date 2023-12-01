-- Tests for basic query dispatch on a hot standy.

-- must show on
-1S: show hot_standby;

-- will be checking if QD/QE info looks good
-1S: select id, type, content, port from gp_backend_info();

----------------------------------------------------------------
-- Test: basic query dispatch
----------------------------------------------------------------
create table hs_t1(a int);
create table hs_t2(a int);
insert into hs_t1 select * from generate_series(1,10);

-- standby should see the result
-1S: select * from hs_t1;

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
-- Test: other things that a hot standby can do
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

-- Here are the things hot standby in PG can do but currently cannot in GPDB:
-- transaction block BEGIN...END;
-1S: begin;
-1S: end;
-- cursor operation due to not supporting BEGIN...END yet;

-- checkpoint is allowed on standby but a restart point is created instead
-1S: checkpoint;

----------------------------------------------------------------
-- Test: things that can't be done on a hot standby in both PG and GDPB:
-- no DML, DDL or anything that generates WAL
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

