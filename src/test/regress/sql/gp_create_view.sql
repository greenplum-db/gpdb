-- start_ignore
drop table if exists sourcetable cascade;
drop view if exists v_sourcetable cascade;
drop view if exists v_sourcetable1 cascade;
-- Tests here check that the order of output rows satisfy the ORDER BY clause
-- in the view definition.  This is a PostgreSQL/GPDB extension that is not
-- part of SQL standard. Since ORCA does not honor ORDER BYs in
-- views/sub-selects, disable ORCA for this test.
set optimizer=off;
-- end_ignore

create table sourcetable 
(
        cn int not null,
        vn int not null,
        pn int not null,
        dt date not null,
        qty int not null,
        prc float not null,
        primary key (cn, vn, pn)
) distributed by (cn,vn,pn);

insert into sourcetable values
  ( 2, 41, 100, '1401-1-1', 1100, 2400),
  ( 1, 10, 200, '1401-3-1', 10, 0),
  ( 3, 42, 200, '1401-4-1', 20, 0),
  ( 1, 20, 100, '1401-5-1', 30, 0),
  ( 1, 33, 300, '1401-5-2', 40, 0),
  ( 1, 51, 400, '1401-6-1', 2, 0),
  ( 2, 50, 400, '1401-6-1', 1, 0),
  ( 1, 31, 500, '1401-6-1', 15, 5),
  ( 3, 32, 500, '1401-6-1', 25, 5),
  ( 3, 30, 600, '1401-6-1', 16, 5),
  ( 4, 43, 700, '1401-6-1', 3, 1),
  ( 4, 40, 800, '1401-6-1', 4, 1);

-- Check that the rows come out in order, if there's an ORDER BY in
-- the view definition.
create view  v_sourcetable as select * from sourcetable order by vn;
select row_number() over(), * from v_sourcetable;

create view v_sourcetable1 as SELECT sourcetable.qty, vn, pn FROM sourcetable union select sourcetable.qty, sourcetable.vn, sourcetable.pn from sourcetable order by qty;
select row_number() over(), * from v_sourcetable1;


-- Check that the row-comparison operator is serialized and deserialized
-- correctly, when it's used in a view. This isn't particularly interesting,
-- compared to all the other expression types, but we happened to have a
-- silly bug that broke this particular case.

create view v_sourcetable2 as
  select a.cn as cn, a.vn as a_vn, b.vn as b_vn, a.pn as a_pn, b.pn as b_pn
  from sourcetable a, sourcetable b
  where row(a.*) < row(b.*)
  and a.cn = 1 and b.cn = 1;
select * from v_sourcetable2;

drop view v_sourcetable2;

-- Greenplum divides the query if it mixes window functions with aggregate
-- functions or grouping, test it here because creating view has a check
-- related to the collation assigning process which is affected by the
-- dividing.
CREATE TEMP TABLE gp_create_view_t1 (f1 smallint, f2 text) DISTRIBUTED RANDOMLY;
CREATE TEMP VIEW window_and_agg_v1 AS SELECT count(*) OVER (PARTITION BY f1), max(f2) FROM gp_create_view_t1 GROUP BY f1;

reset optimizer;
