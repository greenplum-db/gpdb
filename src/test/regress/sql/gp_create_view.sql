-- start_ignore
drop table if exists sourcetable cascade;
drop view if exists v_sourcetable cascade;
drop view if exists v_sourcetable1 cascade;
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
  ( 2, 40, 100, '1401-1-1', 1100, 2400),
  ( 1, 10, 200, '1401-3-1', 1, 0),
  ( 3, 40, 200, '1401-4-1', 1, 0),
  ( 1, 20, 100, '1401-5-1', 1, 0),
  ( 1, 30, 300, '1401-5-2', 1, 0),
  ( 1, 50, 400, '1401-6-1', 1, 0),
  ( 2, 50, 400, '1401-6-1', 1, 0),
  ( 1, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 600, '1401-6-1', 12, 5),
  ( 4, 40, 700, '1401-6-1', 1, 1),
  ( 4, 40, 800, '1401-6-1', 1, 1);

-- Check that the rows come out in order, if there's an ORDER BY in
-- the view definition.
--
-- FIXME: gpdiff will unfortunately mask out any differences in the
-- row order, so this test wouldn't catch a bug in that.
create view  v_sourcetable as select * from sourcetable order by vn;
select * from v_sourcetable;

create view v_sourcetable1 as SELECT sourcetable.qty, vn, pn FROM sourcetable union select sourcetable.qty, sourcetable.vn, sourcetable.pn from sourcetable order by qty;
select * from v_sourcetable1;


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

-- Check that views with gp_dist_random in them will be reconstructed back properly.
CREATE TEMP VIEW view_with_gp_dist_random AS SELECT 1 FROM gp_dist_random('pg_class');
SELECT pg_get_viewdef('view_with_gp_dist_random');
CREATE SCHEMA "schema_view\'.gp_dist_random";
CREATE TABLE "schema_view\'.gp_dist_random"."foo\'.bar" (a int);
CREATE TEMP VIEW view_with_gp_dist_random_special_chars AS SELECT * FROM gp_dist_random(E'"schema_view\\''.gp_dist_random"."foo\\''.bar"');
SELECT pg_get_viewdef('view_with_gp_dist_random_special_chars');
DROP SCHEMA "schema_view\'.gp_dist_random" CASCADE;

-- Check that views containing operator expressions involving arrays have the
-- correct internal representation
CREATE TEMP VIEW view_with_array_op_expr AS SELECT '{1}'::int[] = '{2}'::int[];
SELECT pg_get_viewdef('view_with_array_op_expr');

-- Coerce unknown-type literals to type cstring implicitly
-- we are checking to see if a cstring explicit cast is not erroneously
-- generated when the view is created, a explicit one could not be
-- loaded/created because it's against the Postgres policy.
CREATE VIEW unknown_v1 AS SELECT '2020-12-13'::unknown AS field_unknown;
CREATE VIEW unknown_v2 AS SELECT field_unknown::date FROM unknown_v1;
\d+ unknown_v2
SELECT * FROM unknown_v2;

-- Check unknown type data not parsed as other layouts
CREATE TABLE ut1(c1 character varying(20), c2 text);
CREATE VIEW uv1 AS SELECT 'test', c1 FROM ut1;
INSERT INTO ut1 SELECT * FROM uv1;
