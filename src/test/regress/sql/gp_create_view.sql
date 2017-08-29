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
