drop table if exists r;
create table r(a int, b int);

set enable_bitmapscan=off;
set enable_indexscan=off;

-- force_explain
explain
SELECT attnum::information_schema.cardinal_number 
from pg_attribute 
where attnum > 0 and attrelid = 'r'::regclass;

SELECT attnum::information_schema.cardinal_number 
from pg_attribute 
where attnum > 0 and attrelid = 'r'::regclass;

-- this one should fail
SELECT attnum::information_schema.cardinal_number 
from pg_attribute 
where attrelid = 'r'::regclass;

-- force_explain
explain SELECT *
from (SELECT attnum::information_schema.cardinal_number 
      from pg_attribute 
      where attnum > 0 and attrelid = 'r'::regclass) q
where attnum=2;


SELECT *
from (SELECT attnum::information_schema.cardinal_number 
      from pg_attribute 
      where attnum > 0 and attrelid = 'r'::regclass) q
where attnum=2;

select table_schema, table_name,column_name,ordinal_position
from information_schema.columns
where table_name ='r';


select table_schema, table_name,column_name,ordinal_position
from information_schema.columns
where table_name ='r'
and ordinal_position =1;

-- MPP-25724
create table mpp_25724(mpp_25724_col int) distributed by (mpp_25724_col);

select a.column_name
from information_schema.columns a
where a.table_name
in
(select b.table_name from information_schema.tables b where
	a.column_name like 'mpp_25724_col');

select c.relname
from pg_class c
where c.relname
in
(select b.table_name from information_schema.tables b where
	c.relname like 'mpp_25724');

select a.table_name
from information_schema.tables a
where a.table_name
in
(select b.relname from pg_class b where
	a.table_name like 'mpp_25724');

drop table mpp_25724;

drop table r;
