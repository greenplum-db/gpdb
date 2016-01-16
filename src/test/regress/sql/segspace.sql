--
-- Tests the spill files disk space accounting mechanism
-- 

-- create view to read the segspace value

DROP VIEW IF EXISTS segspace_view_gp_workfile_segspace;
DROP FUNCTION IF EXISTS segspace_view_gp_workfile_segspace_f();

DROP VIEW IF EXISTS segspace_view_gp_workfile_mgr_reset_segspace;
DROP FUNCTION IF EXISTS segspace_view_gp_workfile_mgr_reset_segspace_f();


CREATE FUNCTION segspace_view_gp_workfile_segspace_f()
RETURNS SETOF record
AS '$libdir/gp_workfile_mgr', 'gp_workfile_mgr_used_diskspace'
LANGUAGE C IMMUTABLE;


CREATE VIEW segspace_view_gp_workfile_segspace AS
SELECT C.*
FROM gp_toolkit.__gp_localid, segspace_view_gp_workfile_segspace_f() AS C (
segid int,
size bigint
)
UNION ALL
SELECT C.*
FROM gp_toolkit.__gp_masterid, segspace_view_gp_workfile_segspace_f() AS C (
segid int,
size bigint
);


-- create helper UDF to reset the segpsace variable
CREATE FUNCTION segspace_view_gp_workfile_mgr_reset_segspace_f()
RETURNS SETOF bigint
AS '$libdir/gp_workfile_mgr', 'gp_workfile_mgr_reset_segspace'
LANGUAGE C IMMUTABLE;

CREATE VIEW segspace_view_gp_workfile_mgr_reset_segspace AS
SELECT * FROM gp_toolkit.__gp_localid, segspace_view_gp_workfile_mgr_reset_segspace_f()
UNION ALL
SELECT * FROM gp_toolkit.__gp_masterid, segspace_view_gp_workfile_mgr_reset_segspace_f();


--- create and populate the table

DROP TABLE IF EXISTS segspace_test_hj_skew;
CREATE TABLE segspace_test_hj_skew (i1 int, i2 int, i3 int, i4 int, i5 int, i6 int, i7 int, i8 int);

set gp_autostats_mode = none;

-- many values with i1 = 1
INSERT INTO segspace_test_hj_skew SELECT 1,i,i,i,i,i,i,i FROM generate_series (0,99999) i;
-- some nicely distributed values
INSERT INTO segspace_test_hj_skew SELECT i,i,i,i,i,i,i,i FROM generate_series (0,199999) i;

ANALYZE segspace_test_hj_skew;


-- reset the segspace value
-- start_ignore
select count(*) > 0 from segspace_view_gp_workfile_mgr_reset_segspace;
-- end_ignore

--
--  Testing that query cancelation during spilling updates the accounting
--

------------ Interrupting SELECT query that spills -------------------

-- enable the fault injector
--start_ignore
\! gpfaultinjector -f exec_hashjoin_new_batch -y reset --seg_dbid 2
\! gpfaultinjector -f exec_hashjoin_new_batch -y interrupt --seg_dbid 2
--end_ignore

set gp_workfile_type_hashjoin=buffile;
set statement_mem=2048;
set gp_autostats_mode = none;

begin;
SELECT t1.* FROM segspace_test_hj_skew AS t1, segspace_test_hj_skew AS t2 WHERE t1.i1=t2.i2;
rollback;

-- check used segspace after test
reset statement_mem;
select max(size) from segspace_view_gp_workfile_segspace;


------------ Interrupting INSERT INTO query that spills -------------------

-- enable the fault injector
--start_ignore
\! gpfaultinjector -f exec_hashjoin_new_batch -y reset --seg_dbid 2
\! gpfaultinjector -f exec_hashjoin_new_batch -y interrupt --seg_dbid 2
--end_ignore

drop table if exists segspace_t1_created;
create table segspace_t1_created (i1 int, i2 int, i3 int, i4 int, i5 int, i6 int, i7 int, i8 int);

set gp_workfile_type_hashjoin=buffile;
set statement_mem=2048;
set gp_autostats_mode = none;

begin;

insert into segspace_t1_created
SELECT t1.* FROM segspace_test_hj_skew AS t1, segspace_test_hj_skew AS t2 WHERE t1.i1=t2.i2;

rollback;

-- check used segspace after test
reset statement_mem;
select max(size) from segspace_view_gp_workfile_segspace;

--start_ignore
drop table if exists segspace_t1_created;
--end_ignore

------------ Interrupting CREATE TABLE AS query that spills -------------------

-- enable the fault injector
--start_ignore
\! gpfaultinjector -f exec_hashjoin_new_batch -y reset --seg_dbid 2
\! gpfaultinjector -f exec_hashjoin_new_batch -y interrupt --seg_dbid 2
--end_ignore

drop table if exists segspace_t1_created;
set gp_workfile_type_hashjoin=buffile;
set statement_mem=2048;
set gp_autostats_mode = none;

begin;

create table segspace_t1_created AS
SELECT t1.* FROM segspace_test_hj_skew AS t1, segspace_test_hj_skew AS t2 WHERE t1.i1=t2.i2;

rollback;

-- check used segspace after test
reset statement_mem;
select max(size) from segspace_view_gp_workfile_segspace;
