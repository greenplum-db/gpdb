--
-- Queries that lead to hanging (not dead lock) when we don't handle synchronization properly in shared scan
-- Queries that lead to wrong result when we don't finish executing the subtree below the shared scan being squelched.
--

-- start_ignore
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
-- end_ignore

CREATE SCHEMA shared_scan;

SET search_path = shared_scan;

CREATE TABLE foo (a int, b int);
CREATE TABLE bar (c int, d int);
CREATE TABLE jazz(e int, f int);

INSERT INTO foo values (1, 2);
INSERT INTO bar SELECT i, i from generate_series(1, 100)i;
INSERT INTO jazz VALUES (2, 2), (3, 3);

ANALYZE foo;
ANALYZE bar;
ANALYZE jazz;

SELECT $query$
SELECT * FROM
        (
        WITH cte AS (SELECT * FROM foo)
        SELECT * FROM (SELECT * FROM cte UNION ALL SELECT * FROM cte)
        AS X
        JOIN bar ON b = c
        ) AS XY
        JOIN jazz on c = e AND b = f;
$query$ AS qry \gset

-- We are very particular about this plan shape and data distribution with ORCA:
-- 1. `jazz` has to be the inner table of the outer HASH JOIN, so that on a
-- segment which has zero tuples in `jazz`, the Sequence node that contains the
-- Shared Scan will be squelched on that segment. If `jazz` is not on the inner
-- side, the above mentioned "hang" scenario will not be covered.
-- 2. The Shared Scan producer has to be on a different slice from consumers,
-- and some tuples coming out of the Share Scan producer on one segments are
-- redistributed to a different segment over Motion. If not, the above mentioned
-- "wrong result" scenario will not be covered.
EXPLAIN (COSTS OFF)
:qry ;

SET statement_timeout = '15s';

:qry ;

RESET statement_timeout;

SELECT COUNT(*)
FROM (SELECT *,
        (
        WITH cte AS (SELECT * FROM jazz WHERE jazz.e = bar.c)
        SELECT 1 FROM cte c1, cte c2
        )
      FROM bar) as s;

CREATE TABLE t1 (a int, b int);
CREATE TABLE t2 (a int);

-- ORCA plan contains a Shared Scan producer with a unsorted Motion below it
EXPLAIN (COSTS OFF)
WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 LIMIT 10) SELECT a, 1, 1 FROM cte JOIN t2 USING (a);
-- This functions returns one more column than expected.
CREATE OR REPLACE FUNCTION col_mismatch_func1() RETURNS TABLE (field1 int, field2 int)
LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
DECLARE
   v_qry text;
BEGIN
   v_qry := 'WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 LIMIT 10) SELECT a, 1 , 1 FROM cte JOIN t2 USING (a)';
  RETURN QUERY EXECUTE v_qry;
END
$$;

-- This should only ERROR and should not SIGSEGV
SELECT col_mismatch_func1();

-- ORCA plan contains a Shared Scan producer with a sorted Motion below it
EXPLAIN (COSTS OFF)
WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 ORDER BY b LIMIT 10) SELECT a, 1, 1 FROM cte JOIN t2 USING (a);
--- This functions returns one more column than expected.
CREATE OR REPLACE FUNCTION col_mismatch_func2() RETURNS TABLE (field1 int, field2 int)
    LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
DECLARE
    v_qry text;
BEGIN
    v_qry := 'WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 ORDER BY b LIMIT 10) SELECT a, 1 , 1 FROM cte JOIN t2 USING (a)';
    RETURN QUERY EXECUTE v_qry;
END
$$;

-- This should only ERROR and should not SIGSEGV
SELECT col_mismatch_func2();

-- https://github.com/greenplum-db/gpdb/issues/12701
-- Disable cte sharing in subquery
drop table if exists pk_list;
create table pk_list (id int, schema_name varchar, table_name varchar) distributed by (id);
drop table if exists calender;
create table calender (id int, data_hour timestamp) distributed by (id);

explain (costs off)
with
	tbls as (select distinct schema_name, table_name as table_nm from pk_list),
	tbls_daily_report_23 as (select unnest(string_to_array('mart_cm.card' ,',')) as table_nm_23),
	tbls_w_onl_actl_data as (select unnest(string_to_array('mart_cm.cont_resp,mart_cm.card', ',')) as table_nm_onl_act)
select  data_hour, stat.schema_name as schema_nm, dt.table_nm
from (
	select * from calender c
	cross join tbls
) dt
inner join (
	select tbls.schema_name, tbls.table_nm as table_name
	from tbls tbls
) stat on dt.table_nm = stat.table_name
where
	(data_hour = date_trunc('day',data_hour) and stat.schema_name || '.' ||stat.table_name not in (select table_nm_23 from tbls_daily_report_23))
	and (stat.schema_name || '.' ||stat.table_name not in (select table_nm_onl_act from tbls_w_onl_actl_data))
	or (stat.schema_name || '.' ||stat.table_name in (select table_nm_onl_act from tbls_w_onl_actl_data));

-- Test the scenario which already opened many fds
-- start_ignore
RESET search_path;
-- end_ignore
\! mkdir -p /tmp/_gpdb_fault_inject_tmp_dir/

select gp_inject_fault('inject_many_fds_for_shareinputscan', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = 0;
-- borrow the test query in gp_aggregates
select case when ten < 5 then ten else ten * 2 end, count(distinct two), count(distinct four) from tenk1 group by 1;
select gp_inject_fault('inject_many_fds_for_shareinputscan', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;

\! rm -rf /tmp/_gpdb_fault_inject_tmp_dir/

-- To be able to pass this test, Shared Scan's Material node should be marked
-- as cross-slice. Previously, we processed all subplans separately from main
-- plan, which caused Material node not marked as cross-slice (upper main plan's
-- slice1 motion was ignored in processing).
-- No error like "ERROR:  cannot execute inactive Motion (nodeMotion.c:264)"
-- should be shown.
create table t1 (a int, b int, c int) distributed by (a);
explain (costs off)
with cte1 as (
  select max(c) as c from t1
), 
cte2 as (
  select d as c
  from generate_series(
    (select c from cte1) - 4,
    (select c from cte1), 1) d
)
select l.c from cte2 l
left join t1 u on l.c = u.c;

with cte1 as (
  select max(c) as c from t1
), 
cte2 as (
  select d as c
  from generate_series(
    (select c from cte1) - 4,
    (select c from cte1), 1) d
)
select l.c from cte2 l
left join t1 u on l.c = u.c;

-- This case shows flacky count(*) result on pre-patched version. To make it
-- stable wrong we use new fault point.
-- Shared Scan consumer from slice1 not marked as cross-slice and not
-- initialized tuple store. We got '1' as the result of the query - only
-- Shared Scan from slice2 (the part below UNION) executed correctly.
-- From now, cross-slice interaction detection fixed for subplans and we have
-- stable '100' as a result.
create table t2(i int, j int) distributed by (i);
insert into t2 select i, i * 10 from generate_series(1, 10) i;

select gp_inject_fault('material_pre_tuplestore_flush', 'reset', dbid)
from gp_segment_configuration where role = 'p' and content = -1;
select gp_inject_fault('material_pre_tuplestore_flush',
       'sleep', '', '', '', 1, 1, 5, dbid)
from gp_segment_configuration where role = 'p' and content = -1;       
set optimizer_parallel_union = on;

explain (costs off)
with cte1 as (
  select max(j) as max_j from t2
)
select count(*) c
from (
  select s * 10 s
  from generate_series(1, (select max_j from cte1)) s
  union
  select max_j from cte1
) t;

with cte1 as (
  select max(j) as max_j from t2
)
select count(*) c
from (
  select s * 10 s
  from generate_series(1, (select max_j from cte1)) s
  union
  select max_j from cte1
) t;

reset optimizer_parallel_union;
select gp_inject_fault_infinite('material_pre_tuplestore_flush', 'reset', dbid)
from gp_segment_configuration where role = 'p' and content = -1;
