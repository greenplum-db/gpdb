create table ao_basic_t1 (a int, b varchar) using ao_row distributed by (a);

-- Validate that the appendoptimized table access method will be used
-- for this table.
select amhandler from pg_class c, pg_am a where c.relname = 'ao_basic_t1' and c.relam = a.oid;
select count(*) = 1 from pg_appendonly where relid = 'ao_basic_t1'::regclass;

insert into ao_basic_t1 values (1, 'abc'), (2, 'pqr'), (3, 'lmn');

insert into ao_basic_t1 select i, i from generate_series(1,12)i;

select * from ao_basic_t1;

select gp_segment_id, * from ao_basic_t1;

select gp_segment_id, count(*) from ao_basic_t1 group by gp_segment_id;

insert into ao_basic_t1 values (1, repeat('abc', 100000));

-- create two segment files by inserting from concurrent sessions
begin;
insert into ao_basic_t1 select i, 'session1' from generate_series(1,20)i;

\! psql -d regression -c "insert into ao_basic_t1 select i, 'session2' from generate_series(1,20)i"

end;

select a, length(b) from ao_basic_t1;
create index i_ao_basic_t1 on ao_basic_t1 using btree(a);

select blkdirrelid > 0 as blockdir_created from pg_appendonly where relid = 'ao_basic_t1'::regclass;

set enable_seqscan = off;
set enable_bitmapscan = on;
set enable_indexscan = on;
explain (costs off) select * from ao_basic_t1 where a < 4;
select a, length(b) from ao_basic_t1 where a < 4;
insert into ao_basic_t1 select i, 'index insert' from generate_series(1,12)i;

create table heap_t2(a int, b varchar) distributed by (a);
insert into heap_t2 select i, i from generate_series(1, 20)i;

select * from ao_basic_t1 t1 join heap_t2 t2 on t1.a=t2.a where t1.a != 1;

create table ao_basic_t2 (a int) using ao_row distributed by (a);

insert into ao_basic_t2 select i from generate_series(1,20)i;

alter table ao_basic_t2 add column b varchar default 'abc';

select * from ao_basic_t2;

insert into ao_basic_t2 select  i, 'new column' from generate_series(1,12)i;

select * from ao_basic_t2 where b != 'abc';

create table ao_ctas using ao_row as select * from heap_t2;
select amhandler from pg_class c, pg_am a where c.relname = 'ao_ctas' and c.relam = a.oid;

insert into ao_ctas values (0, 'inserted');
table ao_ctas;

insert into ao_ctas select * from heap_t2;
table ao_ctas;

--delete
delete from ao_ctas where a < 5;
select * from ao_ctas;

delete from ao_basic_t1 where a in (1, 10, 4);
select * from ao_basic_t1 where a = 1;
