--
-- DISTRIBUTED TRANSACTIONS
--
--SET debug_print_full_dtm=true;
--
-- start_matchsubs
--
-- # create a match/subs expression
--
-- m/(ERROR|WARNING|CONTEXT|NOTICE):.*The previous session was reset because its gang was disconnected/
-- s/session id \=\s*\d+/session id \= DUMMY/gm
--
-- m/NOTICE:  exchanged partition .*/
-- s/pg_temp_\d+/pg_temp_DUMMY/gm
--
-- end_matchsubs
--
--
-- We want to have an error between the point where all segments are prepared and our decision 
-- to write the Distributed Commit record.
--

-- start_ignore
BEGIN;
DROP TABLE IF EXISTS distxact1_1;
DROP TABLE IF EXISTS distxact1_2;
DROP TABLE IF EXISTS distxact1_3;
DROP TABLE IF EXISTS distxact1_4;
CREATE TABLE distxact1_1 (a int) DISTRIBUTED BY (a);
CREATE TABLE distxact1_2 (a int) DISTRIBUTED BY (a);
CREATE TABLE distxact1_3 (a int) DISTRIBUTED BY (a);
CREATE TABLE distxact1_4 (a int) DISTRIBUTED BY (a);
COMMIT;
-- end_ignore
set optimizer_print_missing_stats = off;
BEGIN;
INSERT INTO distxact1_1 VALUES (1);
INSERT INTO distxact1_1 VALUES (2);
INSERT INTO distxact1_1 VALUES (3);
INSERT INTO distxact1_1 VALUES (4);
INSERT INTO distxact1_1 VALUES (5);
INSERT INTO distxact1_1 VALUES (6);
INSERT INTO distxact1_1 VALUES (7);
INSERT INTO distxact1_1 VALUES (8);
SET debug_abort_after_distributed_prepared = true;
COMMIT;
RESET debug_abort_after_distributed_prepared;

SELECT * FROM distxact1_1;

--
-- We want to have an error during the prepare which will cause a Abort-Some-Prepared broadcast 
-- to cleanup.
--
BEGIN;
INSERT INTO distxact1_2 VALUES (21);
INSERT INTO distxact1_2 VALUES (22);
INSERT INTO distxact1_2 VALUES (23);
INSERT INTO distxact1_2 VALUES (24);
INSERT INTO distxact1_2 VALUES (25);
INSERT INTO distxact1_2 VALUES (26);
INSERT INTO distxact1_2 VALUES (27);
INSERT INTO distxact1_2 VALUES (28);
SET debug_dtm_action_segment=1;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "protocol";
SET debug_dtm_action_protocol = "prepare";
COMMIT;
RESET debug_dtm_action;
RESET debug_dtm_action_target;
RESET debug_dtm_action_protocol;

SELECT * FROM distxact1_2;

--
-- We want to have an error during the commit-prepared broadcast which will cause a
-- Retry-Commit-Prepared broadcast to cleanup.
--
BEGIN;
INSERT INTO distxact1_3 VALUES (31);
INSERT INTO distxact1_3 VALUES (32);
INSERT INTO distxact1_3 VALUES (33);
INSERT INTO distxact1_3 VALUES (34);
INSERT INTO distxact1_3 VALUES (35);
INSERT INTO distxact1_3 VALUES (36);
INSERT INTO distxact1_3 VALUES (37);
INSERT INTO distxact1_3 VALUES (38);
SET debug_dtm_action_segment=1;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "protocol";
SET debug_dtm_action_protocol = "commit_prepared";
COMMIT;
SELECT * FROM distxact1_3;
RESET debug_dtm_action_segment;
RESET debug_dtm_action;
RESET debug_dtm_action_target;
RESET debug_dtm_action_protocol;

--
-- VARIANT of we want to have an error between the point where all segments are prepared and our decision 
-- to write the Distributed Commit record.  Cause problem during abort-prepared broadcast.  
--
BEGIN;
INSERT INTO distxact1_4 VALUES (41);
INSERT INTO distxact1_4 VALUES (42);
INSERT INTO distxact1_4 VALUES (43);
INSERT INTO distxact1_4 VALUES (44);
INSERT INTO distxact1_4 VALUES (45);
INSERT INTO distxact1_4 VALUES (46);
INSERT INTO distxact1_4 VALUES (47);
INSERT INTO distxact1_4 VALUES (48);
SET debug_dtm_action_segment=1;
SET debug_abort_after_distributed_prepared = true;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "protocol";
SET debug_dtm_action_protocol = "abort_prepared";
COMMIT;
SELECT * FROM distxact1_4;
RESET debug_dtm_action_segment;
RESET debug_abort_after_distributed_prepared;
RESET debug_dtm_action;
RESET debug_dtm_action_target;
RESET debug_dtm_action_protocol;

--
-- Fail general commands
--


--
-- Invoke a failure during a CREATE TABLE command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action_segment=1;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "MPPEXEC UTILITY";
CREATE TABLE distxact2_1 (a int);
RESET debug_dtm_action_segment;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact2_1;

-- Should succeed
CREATE TABLE distxact2_1 (a int);
DROP TABLE distxact2_1;


--
-- Invoke a failure during a CREATE TABLE command.  
-- Action_Target = 2 is SQL.
--
SET debug_dtm_action_segment=1;
SET debug_dtm_action = "fail_end_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "MPPEXEC UTILITY";
CREATE TABLE distxact2_2 (a int);
RESET debug_dtm_action_segment;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact2_2;

-- Should succeed
CREATE TABLE distxact2_2 (a int);
DROP TABLE distxact2_2;


--
-- xact.c DTM related dispatches
--

--
-- Invoke a failure during a SAVEPOINT command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action_segment=1;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "SAVEPOINT";
BEGIN;
CREATE TABLE distxact3_1 (a int);
SAVEPOINT s;
ROLLBACK;
RESET debug_dtm_action_segment;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact3_1;

-- Should succeed
CREATE TABLE distxact3_1 (a int);
DROP TABLE distxact3_1;


--
-- Invoke a failure during a RELEASE SAVEPOINT command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action_segment=1;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "RELEASE";
BEGIN;
CREATE TABLE distxact3_2 (a int);
SAVEPOINT s;
INSERT INTO distxact3_2 VALUES (21);
INSERT INTO distxact3_2 VALUES (22);
INSERT INTO distxact3_2 VALUES (23);
INSERT INTO distxact3_2 VALUES (24);
INSERT INTO distxact3_2 VALUES (25);
INSERT INTO distxact3_2 VALUES (26);
INSERT INTO distxact3_2 VALUES (27);
INSERT INTO distxact3_2 VALUES (28);
RELEASE SAVEPOINT s;
ROLLBACK;
RESET debug_dtm_action_segment;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact3_2;

-- Should succeed
CREATE TABLE distxact3_2 (a int);
DROP TABLE distxact3_2;



--
-- Invoke a failure during a ROLLBACK TO SAVEPOINT command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action_segment=1;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "ROLLBACK";
BEGIN;
CREATE TABLE distxact3_3 (a int);
SAVEPOINT s;
INSERT INTO distxact3_3 VALUES (31);
INSERT INTO distxact3_3 VALUES (32);
INSERT INTO distxact3_3 VALUES (33);
INSERT INTO distxact3_3 VALUES (34);
INSERT INTO distxact3_3 VALUES (35);
INSERT INTO distxact3_3 VALUES (36);
INSERT INTO distxact3_3 VALUES (37);
INSERT INTO distxact3_3 VALUES (38);
ROLLBACK TO SAVEPOINT s;
ROLLBACK;
RESET debug_dtm_action_segment;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact3_3;

-- Should succeed
CREATE TABLE distxact3_3 (a int);
DROP TABLE distxact3_3;

RESET debug_print_full_dtm;


-- Test cursor/serializable interaction.
-- MPP-3227: pg_dump does this exact sequence.
-- for each table in a database.

drop table if exists dtmcurse_foo;
drop table if exists dtmcurse_bar;

create table dtmcurse_foo (a int, b int);
insert into dtmcurse_foo values (1,2);
insert into dtmcurse_foo values (2,2);

create table dtmcurse_bar as select * from dtmcurse_foo distributed by (b);

begin;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
DECLARE cursor1 CURSOR FOR SELECT * FROM ONLY dtmcurse_foo order by a;
fetch 1 from cursor1;
close cursor1;
-- MPP-3227: second declare would hang waiting for snapshot,
-- should work just like the first.
DECLARE cursor1 CURSOR FOR SELECT * FROM ONLY dtmcurse_bar order by a;
fetch 1 from cursor1;
close cursor1;
abort;

-- MPP-4504: cursor + InitPlan
begin;
declare c1 cursor for select * from dtmcurse_foo where a = (select min(a) from dtmcurse_foo);
fetch 1 from c1;
close c1;
end;

drop table if exists dtmcurse_foo;
drop table if exists dtmcurse_bar;

-- Test distribute transaction if 'COMMIT/END' is included in a multi-queries command.
\! psql postgres -c "begin;end; create table dtx_test1(c1 int); drop table dtx_test1;"

-- Test two phase commit for extended query
\! ./twophase_pqexecparams dbname=regression

--
-- Subtransactions with partition table DDLs.
--
BEGIN;
Create table subt_alter_part_tab_ao1 (
 i int, x text, c char, v varchar, d date, n numeric,
 t timestamp without time zone, tz time with time zone)
 with (appendonly=true, orientation=column) distributed by (i)
 partition by range (i)
 (partition p1 start(1) end(5),
  partition p2 start(5) end(8),
  partition p3 start(8) end(10));

Savepoint sp1;
Alter table subt_alter_part_tab_ao1 add partition p4 start(10) end(14);
Alter table subt_alter_part_tab_ao1 add default partition def_part;
Insert into subt_alter_part_tab_ao1 values(
 generate_series(1,15), 'create table with subtransactions', 's',
 'subtransaction table', '12-11-2012', 3, '2012-10-09 10:23:54',
 '2011-08-19 10:23:54+02');

Savepoint sp2; -- child of sp1
Select count(*) from subt_alter_part_tab_ao1;
Alter table subt_alter_part_tab_ao1 drop partition p3;
Select count(*) from subt_alter_part_tab_ao1;
select count(*) = 0 as passed from subt_alter_part_tab_ao1
 where i >= 8 and i < 10;

Savepoint sp3; -- child of sp2
Alter table subt_alter_part_tab_ao1 drop default partition;
release savepoint sp3; -- commit sp3
Select count(*) from subt_alter_part_tab_ao1;
select count(*) = 0 as passed from subt_alter_part_tab_ao1 where i > 13;

savepoint sp4; -- child of sp2
Create table exg_pt_ao1(i int, x text,c char,v varchar, d date, n numeric,
 t timestamp without time zone, tz time with time zone) distributed by (i);
Insert into exg_pt_ao1 values (
 7, 'to be exchanged', 's', 'partition table', '12-11-2012', 3,
 '2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
Alter table subt_alter_part_tab_ao1 exchange partition p2
 with table exg_pt_ao1;
select count(*) = 1 as passed from subt_alter_part_tab_ao1
 where i >= 5 and i < 8;

rollback to sp4;
select count(*) > 1 as passed from subt_alter_part_tab_ao1
 where i >= 5 and i < 8;
Alter table subt_alter_part_tab_ao1 split partition p4 at (13) into
 (partition splita,partition splitb);
select count(*) from subt_alter_part_tab_ao1;

savepoint sp5; -- child of sp4
Alter table subt_alter_part_tab_ao1 drop partition splita;
Select count(*) from subt_alter_part_tab_ao1;

rollback to sp5;
Select count(*) from subt_alter_part_tab_ao1;
Alter table subt_alter_part_tab_ao1 drop partition splita;

rollback to sp2; -- should abort committed child sp3
COMMIT;
Select count(*) = 15 as passed from subt_alter_part_tab_ao1;
select count(*) > 0 as passed from subt_alter_part_tab_ao1 where i > 13;
-- DML should work after the subtransaction business.
Insert into subt_alter_part_tab_ao1 values(
 generate_series(1,15), 'create table with subtransactions', 's',
 'subtransaction table', '12-11-2012', 3, '2012-10-09 10:23:54',
 '2011-08-19 10:23:54+02');
Select count(*) = 30 as passed from subt_alter_part_tab_ao1;

--
-- Subtransactions with DDL/DMLs on append-optimized tables.
--
BEGIN;
Create table subt_alter_table_ao1 (col0 int)
 with (appendonly=true) distributed by (col0);
Create table subt_alter_table_co1 (col0 int)
 with (appendonly=true, orientation=column) distributed by (col0);
Insert into subt_alter_table_ao1 values(generate_series(1,5));
Insert into subt_alter_table_co1 values(generate_series(1,5));

Savepoint sp1;
-- Add column, update, commit subtransaction.
Alter table subt_alter_table_ao1 add column col1 int default 10;
update subt_alter_table_ao1 set col1 = col0;
Alter table subt_alter_table_co1 add column col1 int default 10;
update subt_alter_table_co1 set col1 = col0;
release savepoint sp1;

-- Alter column type, update, rollback subtransaction.
Savepoint sp2;
Alter table subt_alter_table_ao1 drop column col1;
insert into subt_alter_table_ao1 values (6), (7);
select count(attnum) = 1 as passed from pg_attribute
 where attrelid = 'subt_alter_table_ao1'::regclass
 and attisdropped = true;
Alter table subt_alter_table_co1 alter column col1 type float;
update subt_alter_table_co1 set col1 = col0/10::float;
select count(*) = 5 as passed from subt_alter_table_co1 where col1 < 1;

rollback to sp2;
select count(attnum) = 0 as passed from pg_attribute
 where attrelid = 'subt_alter_table_ao1'::regclass
 and attisdropped = true;
select count(*) = 0 as passed from subt_alter_table_co1 where col1 < 1;
COMMIT;

select count(attnum) = 0 as passed from pg_attribute
 where attrelid = 'subt_alter_table_ao1'::regclass
 and attisdropped = true;
select count(*) = 0 as passed from subt_alter_table_co1 where col1 < 1;

--
-- Subtransactions with reindex and truncate.
--
BEGIN;
-- Enforce index usage for this test.
set enable_seqscan=false;
set enable_indexscan=true;
set enable_bitmapscan=true;
Create table subt_reindex_heap (i int, x text, n numeric, b box)
 distributed by (i);
Create index bt_ri_heap on subt_reindex_heap (x);
Create index bm_ri_heap on subt_reindex_heap using bitmap (n);
Create index gist_ri_heap on subt_reindex_heap using gist (b);
Create Unique index unique_ri_heap on subt_reindex_heap (i);
Create table subt_reindex_ao (i int, x text, n numeric, b box)
 with(appendonly=true) distributed by (i);
Create index bt_ri_ao on subt_reindex_ao (x);
Create index bm_ri_ao on subt_reindex_ao using bitmap (n);
Create index gist_ri_ao on subt_reindex_ao using gist (b);
Create table subt_reindex_co (i int, x text, n numeric, b box)
 with(appendonly=true, orientation=column) distributed by (i);
Create index bt_ri_co on subt_reindex_co (x);
Create index bm_ri_co on subt_reindex_co using bitmap (n);
Create index gist_ri_co on subt_reindex_co using gist (b);
savepoint sp1;
Insert into subt_reindex_heap select i, 'heap '||i, 2,
 ('(0,'||i||', 1,'||i+1||')')::box from generate_series(1,5)i;
Insert into subt_reindex_ao select i, 'AO '||i, 2,
 ('(0,'||i||', 1,'||i+1||')')::box from generate_series(1,5)i;
Insert into subt_reindex_co select i, 'CO '||i, 2,
 ('(0,'||i||', 1,'||i+1||')')::box from generate_series(1,5)i;
savepoint sp2; -- child of sp1
Insert into subt_reindex_heap values
 (6, 'heap 6', 3, '((0,0), (1,1))');
Insert into subt_reindex_ao values
 (5, 'AO 5', 3, '((0,0), (1,1))');
Insert into subt_reindex_co values
 (5, 'CO 5', 3, '((0,0), (1,1))');
update subt_reindex_heap set n = -i where n = 3;
update subt_reindex_ao set n = -i where n = 3;
update subt_reindex_co set n = -i where n = 3;
savepoint sp3; -- child of sp2;
REINDEX index bm_ri_heap;
REINDEX index bm_ri_ao;
REINDEX index bm_ri_co;
select count(*) = 1 as passed from subt_reindex_heap where n < 0;
select count(*) = 1 as passed from subt_reindex_ao where n < 0;
select count(*) = 1 as passed from subt_reindex_co where n < 0;
release savepoint sp3; -- commit sp3
savepoint sp4; -- child of sp2
REINDEX index unique_ri_heap;
REINDEX index bt_ri_ao;
REINDEX index bm_ri_ao;
REINDEX index gist_ri_ao;
REINDEX index bt_ri_co;
REINDEX index bm_ri_co;
REINDEX index gist_ri_co;
savepoint sp5; -- child of sp4
select count(*) = 1 as passed from subt_reindex_heap where x = 'heap 2';
select count(*) = 1 as passed from subt_reindex_ao where x = 'AO 3';
select count(*) = 1 as passed from subt_reindex_co where x = 'CO 4';
select 0/0;
rollback to sp4;
select count(*) = 1 as passed from subt_reindex_heap where i = 1;
select count(*) = 2 as passed from subt_reindex_ao where i = 5;
select count(*) = 2 as passed from subt_reindex_co where i = 5;
update subt_reindex_heap set x = 'heap sp4', b = '((1,1),(4,4))'
 where i = 2;
update subt_reindex_ao set x = 'AO sp4', b = '((1,1),(4,4))'
 where i = 2;
update subt_reindex_co set x = 'CO sp4', b = '((1,1),(4,4))'
 where i = 2;
savepoint sp6; -- child of sp4
REINDEX index bt_ri_heap;
REINDEX index bm_ri_heap;
REINDEX index gist_ri_heap;
REINDEX index unique_ri_heap;
REINDEX index bt_ri_ao;
REINDEX index bt_ri_ao;
REINDEX index gist_ri_ao;
REINDEX index bt_ri_co;
REINDEX index bt_ri_co;
REINDEX index gist_ri_co;
release savepoint sp6;
select count(*) = 1 as passed from subt_reindex_heap
 where b = '((1,1), (4,4))';
select count(*) = 1 as passed from subt_reindex_ao
 where b = '((1,1), (4,4))';
select count(*) = 1 as passed from subt_reindex_co
 where b = '((1,1), (4,4))';

rollback to sp2;

select count(*) = 5 as passed from subt_reindex_heap
 where n = 2;
select count(*) = 5 as passed from subt_reindex_ao
 where n = 2;
select count(*) = 5 as passed from subt_reindex_co
 where n = 2;
select count(*) = 0 as passed from subt_reindex_ao
 where x = 'AO sp4';

-- truncate cases
savepoint sp7; -- child of sp2
truncate subt_reindex_heap;
truncate subt_reindex_ao;
savepoint sp8; -- child of sp7
truncate subt_reindex_co;
select count(*) = 0 as passed from subt_reindex_heap where i < 7;
select count(*) = 0 as passed from subt_reindex_ao where i < 6;
select count(*) = 0 as passed from subt_reindex_co where i < 6;
rollback to sp8;
update subt_reindex_co set x = 'CO sp8', b = '((1,1),(8,8))'
 where i = 2;
release savepoint sp7; -- commit sp7

-- Test rollback of truncate in a committed subtransaction.
rollback to sp2;

COMMIT;

select count(*) = 5 as passed from subt_reindex_heap;
select count(*) = 5 as passed from subt_reindex_ao;
select count(*) = 5 as passed from subt_reindex_co;

-- GPDB has a limitation on REINDEX of catalog tables: you cannot do it in
-- a transaction block. Check for that.
\c postgres
begin;
reindex table pg_class;
commit;
\c regression

--
-- Check that committing a subtransaction releases the lock on the
-- subtransaction's XID.
--
-- It's not too bad if it doesn't, because if anyone wants to wait for the
-- subtransaction and sees that it's been committed already, they will wait
-- for the top transaction XID instead. So even though the lock on the sub-XID
-- is released at RELEASE SAVEPOINT, logically it's held until the end of
-- the top transaction anyway. But releasing the lock early saves space in
-- the lock table. (We had a silly bug once upon a time in GPDB where we failed
-- to release the lock.)
--
BEGIN;
CREATE TEMPORARY TABLE foo (i integer);
DO $$
declare
  i int;
begin
  for i in 1..100 loop
    begin
      insert into foo values (i);
    exception
      when others then raise 'got error';
    end;
  end loop;
end;
$$;
SELECT CASE WHEN count(*) < 50 THEN 'not many XID locks'
            ELSE 'lots of XID locks: ' || count(*) END
FROM pg_locks WHERE locktype='transactionid';
ROLLBACK;

-- Test that exported snapshots are cleared upon abort.  In Greenplum,
-- exported snapshots are cleared earlier than PostgreSQL during
-- abort.
begin;
select count(1) = 1 from pg_catalog.pg_export_snapshot();
select pg_cancel_backend(pg_backend_pid());
rollback;
