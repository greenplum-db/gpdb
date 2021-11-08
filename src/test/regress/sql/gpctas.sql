set optimizer_print_missing_stats = off;
drop table if exists ctas_src;
drop table if exists ctas_dst;

create table ctas_src (domain integer, class integer, attr text, value integer) distributed by (domain);
insert into ctas_src values(1, 1, 'A', 1);
insert into ctas_src values(2, 1, 'A', 0);
insert into ctas_src values(3, 0, 'B', 1);

-- MPP-2859
create table ctas_dst as 
SELECT attr, class, (select count(distinct class) from ctas_src) as dclass FROM ctas_src GROUP BY attr, class distributed by (attr);

drop table ctas_dst;

create table ctas_dst as 
SELECT attr, class, (select max(class) from ctas_src) as maxclass FROM ctas_src GROUP BY attr, class distributed by (attr);

drop table ctas_dst;

create table ctas_dst as 
SELECT attr, class, (select count(distinct class) from ctas_src) as dclass, (select max(class) from ctas_src) as maxclass, (select min(class) from ctas_src) as minclass FROM ctas_src GROUP BY attr, class distributed by (attr);

-- MPP-4298: "unknown" datatypes.
drop table if exists ctas_foo;
drop table if exists ctas_bar;
drop table if exists ctas_baz;

create table ctas_foo as select * from generate_series(1, 100);
create table ctas_bar as select a.generate_series as a, b.generate_series as b from ctas_foo a, ctas_foo b;

create table ctas_baz as select 'delete me' as action, * from ctas_bar distributed by (a);
-- "action" becomes text
\d ctas_baz
select action, b from ctas_baz order by 1,2 limit 5;
select action, b from ctas_baz order by 2 limit 5;
select action::text, b from ctas_baz order by 1,2 limit 5;

alter table ctas_baz alter column action type text;
\d ctas_baz
select action, b from ctas_baz order by 1,2 limit 5;
select action, b from ctas_baz order by 2 limit 5;
select action::text, b from ctas_baz order by 1,2 limit 5;

-- Test CTAS with a function that executes another query that's dispatched.
-- Once upon a time, we had a bug in dispatching the table's OID in this
-- scenario.
create table ctas_input(x int);
insert into ctas_input select * from generate_series(1, 10);

CREATE FUNCTION ctas_inputArray() RETURNS INT[] AS $$
DECLARE theArray INT[];
BEGIN
   SELECT array(SELECT * FROM ctas_input ORDER BY x) INTO theArray;
   RETURN theArray;
--EXCEPTION WHEN OTHERS THEN RAISE NOTICE 'Catching the exception ...%', SQLERRM;
END;
$$ LANGUAGE plpgsql;

create table ctas_output as select ctas_inputArray()::int[] as x;


-- Test CTAS with VALUES.

CREATE TEMP TABLE yolo(i, j, k) AS (VALUES (0,0,0), (1, NULL, 0), (2, NULL, 0), (3, NULL, 0)) DISTRIBUTED BY (i);


--
-- Test that the rows are distributed correctly in CTAS, even if the query
-- has an ORDER BY. This used to tickle a bug at one point.
--

DROP TABLE IF EXISTS ctas_src, ctas_dst;

CREATE TABLE ctas_src(
col1 int4,
col2 decimal,
col3 char,
col4 boolean,
col5 int
) DISTRIBUTED by(col4);

-- I'm not sure why, but dropping a column was crucial to tickling the
-- original bug.
ALTER TABLE ctas_src DROP COLUMN col2;
INSERT INTO ctas_src(col1, col3,col4,col5)
    SELECT g, 'a',True,g from generate_series(1,5) g;

CREATE TABLE ctas_dst as SELECT col1,col3,col4,col5 FROM ctas_src order by 1;

-- This will fail to find some of the rows, if they're distributed incorrectly.
SELECT * FROM ctas_src, ctas_dst WHERE ctas_src.col1 = ctas_dst.col1;

-- Github Issue 9365: https://github.com/greenplum-db/gpdb/issues/9365
-- Previously, the following CTAS case miss dispatching OIDs to QEs, which leads to
-- errors.
CREATE OR REPLACE FUNCTION array_unnest_2d_to_1d(
  x ANYARRAY,
  OUT unnest_row_id INT,
  OUT unnest_result ANYARRAY
)
RETURNS SETOF RECORD
AS
$BODY$
  SELECT t2.r::int, array_agg($1[t2.r][t2.c] order by t2.c) FROM
  (
    SELECT generate_series(array_lower($1,2),array_upper($1,2)) as c, t1.r
    FROM
    (
      SELECT generate_series(array_lower($1,1),array_upper($1,1)) as r
    ) t1
  ) t2
GROUP BY t2.r
$BODY$ LANGUAGE SQL IMMUTABLE
;

DROP TABLE IF EXISTS unnest_2d_tbl01;
CREATE TABLE unnest_2d_tbl01 (id INT, val DOUBLE PRECISION[][]);
INSERT INTO unnest_2d_tbl01 VALUES
  (1, ARRAY[[1::float8,2],[3::float8,4],[5::float8,6],[7::float8,8]]),
  (2, ARRAY[[101::float8,202],[303::float8,404],[505::float8,606]])
;
DROP TABLE IF EXISTS unnest_2d_tbl01_out;
-- The following CTAS fails previously, see Github Issue 9365
CREATE TABLE unnest_2d_tbl01_out AS
  SELECT id, (array_unnest_2d_to_1d(val)).* FROM unnest_2d_tbl01;

-- Github issue 9790.
-- Previously, CTAS with no data won't handle the 'WITH' clause
CREATE TABLE ctas_base(a int, b int);
CREATE TABLE ctas_aocs WITH (appendonly=true, orientation=column) AS SELECT * FROM ctas_base WITH NO DATA;
SELECT * FROM ctas_aocs;
DROP TABLE ctas_base;
DROP TABLE ctas_aocs;

-- Github Issue 10760
-- Previously CTAS with Initplan will dispatch oids twice
-- start_ignore
DROP TABLE IF EXISTS t1_github_issue_10760;
DROP TABLE IF EXISTS t2_github_issue_10760;

create table t1_github_issue_10760(a int, b int) distributed randomly;
-- end_ignore

-- Because there is no Initplan in ORCA of this test case, there is no
-- 10760 problem in ORCA. So here manually set the optimizer to
-- ensure that there is Initplan in the execution plan.
set optimizer=off;
explain select * from t1_github_issue_10760 where b > (select count(*) from t1_github_issue_10760);

begin;
        create table t2_github_issue_10760 as select * from t1_github_issue_10760 where b > (select count(*) from t1_github_issue_10760) distributed randomly;
        drop table t2_github_issue_10760;
        create table t2_github_issue_10760 as select * from t1_github_issue_10760 where b > (select count(*) from t1_github_issue_10760) distributed randomly;
end;

select count (distinct oid) from  (select oid from pg_class where relname = 't2_github_issue_10760' union all select oid from gp_dist_random('pg_class') where relname = 't2_github_issue_10760')x;

drop table t1_github_issue_10760;
drop table t2_github_issue_10760;

reset optimizer;

-- CTAS with no data will not lead to catalog inconsistent
-- See github issue: https://github.com/greenplum-db/gpdb/issues/11999
create or replace function mv_action_select_issue_11999() returns bool language sql as
'declare c cursor for select 1/0; select true';

create materialized view sro_mv_issue_11999 as select mv_action_select_issue_11999() with no data;
create table t_sro_mv_issue_11999 as select mv_action_select_issue_11999() with no data;

select count(*)
from
(
  select localoid, policytype, numsegments, distkey
  from gp_distribution_policy
  where localoid::regclass::text = 'sro_mv_issue_11999' or
        localoid::regclass::text = 't_sro_mv_issue_11999'
  union all
  select localoid, policytype, numsegments, distkey
  from gp_dist_random('gp_distribution_policy')
  where localoid::regclass::text = 'sro_mv_issue_11999' or
        localoid::regclass::text = 't_sro_mv_issue_11999'
)x;

select count(distinct (localoid, policytype, numsegments, distkey))
from
(
  select localoid, policytype, numsegments, distkey
  from gp_distribution_policy
  where localoid::regclass::text = 'sro_mv_issue_11999' or
        localoid::regclass::text = 't_sro_mv_issue_11999'
  union all
  select localoid, policytype, numsegments, distkey
  from gp_dist_random('gp_distribution_policy')
  where localoid::regclass::text = 'sro_mv_issue_11999' or
        localoid::regclass::text = 't_sro_mv_issue_11999'
)x;

-- then refresh should error out
refresh materialized view sro_mv_issue_11999;


-- Test CTAS + initplan, and an exception was raised in preprocess_initplans
CREATE OR REPLACE FUNCTION public.exception_func()
 RETURNS refcursor
 LANGUAGE plpgsql
AS $function$declare cname refcursor = 'result'; begin open cname for select 1; raise sqlstate '02000'; return cname; exception when sqlstate '02000' then  return cname; end;$function$;

SELECT exception_func() INTO TEMPORARY test_tmp1;

SELECT * FROM test_tmp1;

CREATE TEMPORARY TABLE test_tmp2 AS SELECT exception_func();

SELECT * FROM test_tmp2;

DROP FUNCTION public.exception_func();
DROP TABLE test_tmp1;
DROP TABLE test_tmp2;
