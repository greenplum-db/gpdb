-- this SQL suites only test planners unique rowid path
-- set fault inject at the start of the file and reset
-- it when finish. This SQL tests planner but let's also
-- run the queries under ORCA (means without set optimizer = off here).
create extension if not exists gp_inject_fault;
select gp_inject_fault_infinite('low_unique_rowid_path_cost', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = -1;

-- Test index only scan path not error out when semjoin.
-- Greenplum might add unique_rowid_path to handle semjoin,
-- that was introduced in Greenplum long before, and after
-- merging so many commits from upstream, new logic might
-- not work well. The following cases test for this.
create table t_indexonly_cdbdedup_1(a int, b int, c int);
create table t_indexonly_cdbdedup_2(a int, b int, c int);
create index t_indexonly_cdbdedup_2_index on t_indexonly_cdbdedup_2(b);

set enable_seqscan = off;
set enable_bitmapscan = off;

explain (costs off)
select b from t_indexonly_cdbdedup_2 where b in (select b from t_indexonly_cdbdedup_1 where t_indexonly_cdbdedup_1.b = 3) ;

reset enable_bitmapscan;
reset enable_seqscan;

-- Test append path not error out when semjoin.
-- issue1:https://github.com/greenplum-db/gpdb/issues/12402
-- issue2:https://github.com/greenplum-db/gpdb/issues/3719

-- Greenplum might add unique_rowid_path to handle semjoin, that
-- was introduced in Greenplum long before, and after merging so
-- many commits from upstream, new logic might not work well.
-- We just disallow unique_rowid_path for inheritance_planner.

-- through injecting fault in unique_row_path to make the cost
-- of unique_rowid low, however, we use a switch to disallow
-- create unique_rowid plan for inheritance plan.
-- the two above issues can work well.
-- The following cases test for this.

--create table
create table rank_12402 (id int, rank int, year int, value int) distributed by (id)
partition by range (year) (start (2006) end (2007) every (1), default partition extra );

create table rank1_12402 (id int, rank int, year int, value int) distributed by (id)
partition by range (year) (start (2006) end (2007) every (1), default partition extra );

-- It should create a unique_rowid plan.
-- but it creates a semi-join plan, due to we disallow unique_rowid path in inheritance_planner.
explain (costs off ) update rank_12402 set rank = 1 where id in (select id from rank1_12402) and value in (select value from rank1_12402);

-- Test for fake ctid works well
-- issue: https://github.com/greenplum-db/gpdb/issues/12512
-- NOTE: orca use split-update, planner use update, behavior is different when
-- tuple is updated by self.

-- test ctid for subquery in update
create table t_12512(a int, b int, c int);
create table t1_12512(a int, b int, c int);
create table t2_12512(a int, b int, c int);

insert into t_12512 select i,i,i from generate_series(1, 100)i;
insert into t1_12512 select i,i,i from generate_series(1, 100)i;
insert into t2_12512 select i,i,i from generate_series(1, 100)i;

explain (costs off)
update t_12512 set b = 1
    from
(
          select
            t1_12512.b, sum(t1_12512.a) as x from t1_12512 group by t1_12512.b
)e
where e.x in
    (
    select b from t2_12512
    )
;

update t_12512 set b = 1
    from
(
          select
            t1_12512.b, sum(t1_12512.a) as x from t1_12512 group by t1_12512.b
)e
where e.x in
    (
    select b from t2_12512
    )
;

-- test fake ctid for functions
explain (costs off)
update t_12512 set b = 1
    from
(
	  select * from pg_backend_pid() x

)e
where e.x in
    (
    select b from t2_12512
    )
;

update t_12512 set b = 1
    from
(
	  select * from pg_backend_pid() x

)e
where e.x in
    (
    select b from t2_12512
    )
;

-- test fake ctid for values scan
explain (costs off)
update t_12512 set b = 1
    from
(
	  select x from (values (1), (2), (3), (4)) as z(x)

)e
where e.x in
    (
    select b from t2_12512
    )
;

update t_12512 set b = 1
    from
(
	  select x from (values (1), (2), (3), (4)) as z(x)

)e
where e.x in
    (
    select b from t2_12512
    )
;

-- test fake ctid for external scan

CREATE OR REPLACE FUNCTION write_to_file_12512() RETURNS integer AS
   '$libdir/gpextprotocol.so', 'demoprot_export' LANGUAGE C STABLE;
CREATE OR REPLACE FUNCTION read_from_file_12512() RETURNS integer AS
    '$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE;

-- declare the protocol name along with in/out funcs
CREATE PROTOCOL demoprot (
    readfunc  = read_from_file_12512,
    writefunc = write_to_file_12512
);

CREATE writable EXTERNAL TABLE ext_w_12512(like t2_12512)
    LOCATION('demoprot://demotextfile.txt')
FORMAT 'text'
DISTRIBUTED BY (a);

INSERT into ext_w_12512 select * from t2_12512;

CREATE  EXTERNAL TABLE ext_r_12512(like t2_12512) LOCATION('demoprot://demotextfile.txt') FORMAT 'text';

explain (costs off)
update t_12512 set b = 1
    from
(
	  select a x, b from ext_r_12512
)e
where e.x in
    (
    select b from t2_12512
    )
;

update t_12512 set b = 1
    from
(
	  select a x, b from ext_r_12512
)e
where e.x in
    (
    select b from t2_12512
    )
;

-- reset fault injector
select gp_inject_fault('low_unique_rowid_path_cost', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

-- clean up
drop index t_indexonly_cdbdedup_2_index;
drop table t_indexonly_cdbdedup_1;
drop table t_indexonly_cdbdedup_2;
drop table rank_12402;
drop table rank1_12402;
drop table t_12512;
drop table t1_12512;
drop table t2_12512;
drop external table ext_w_12512;
drop external table ext_r_12512;
drop protocol demoprot;
drop function write_to_file_12512();
drop function read_from_file_12512();
