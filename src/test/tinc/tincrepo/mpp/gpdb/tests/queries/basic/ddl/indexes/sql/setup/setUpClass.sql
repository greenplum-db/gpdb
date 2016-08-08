create language plpythonu;
drop table heap_tab;
drop table ao_tab;
drop table aoco_tab;
drop table heapaoco_part;
drop table aoco_part;

create or replace function insert_rows(work_mem int, table_name text) returns text as
$$
query = "select count(*) from gp_segment_configuration where preferred_role = 'p' and content != -1"
rv = plpy.execute(query)
num_seg = int(rv[0]['count'])
num_rows = (work_mem / 8216) * 32768 * num_seg
query = "insert into %s select generate_series(1, %d)" % (table_name, int(num_rows) * 2)
rv = plpy.execute(query)
return num_rows * 2
$$
language plpythonu;

create table heap_tab (i int);
create table ao_tab (i int) with (appendonly = true);
create table aoco_tab (i int) with (appendonly = true, orientation=column);
create table heapaoco_part (i int) partition by list(i)
(partition p1 values(1,3,5), partition p2 values(2,4,6) with (appendonly=true), partition p3 values(7,9,11), partition p4 values(8,10,12) with (appendonly=true), 
partition p5 values(13,15,17) with (appendonly=true, orientation=column), partition p7 values(14,16,18), default partition other with (appendonly=true));
create table aoco_part (i int) with(appendonly=true) partition by range(i)
(partition p1 start (0) end (50000000) every (7000000), default partition other with (appendonly=true, orientation=column));

select insert_rows(256000, 'heap_tab');
insert into ao_tab select * from heap_tab;
insert into aoco_tab select * from heap_tab;
insert into heapaoco_part select * from heap_tab;
insert into aoco_part select * from heap_tab;
