-- Reorder children in pg_inherits for parent relation 'co1'
create table deleteme as select * from pg_inherits
 where inhparent = 'co1'::regclass and
 (inhrelid = 'co1_1_prt_aa'::regclass or
  inhrelid = 'co1_1_prt_bb'::regclass);
delete from pg_inherits where
 (inhrelid = 'co1_1_prt_aa'::regclass or
  inhrelid = 'co1_1_prt_bb'::regclass);
insert into pg_inherits select * from deleteme;
drop table deleteme;
select inhrelid::regclass from pg_inherits
 where inhparent = 'co1'::regclass;
