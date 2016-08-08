-- 
-- @created 2015-07-11 12:00:00
-- @modified 2015-07-11 12:00:00
-- @tags external_partition 
-- @gpdiff true 
-- @description Tests for exchange partition 

--start_ignore
drop table if exists pt_ext;
CREATE TABLE pt_ext
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

insert into pt_ext select i,i,'test',true from generate_series(1,50) i;
create temp table tmp as select * from pt_ext where col1 < 11;
\! rm /tmp/exttab_list
copy tmp to '/tmp/exttab_list' csv;
create readable external table ret(like pt_ext) location('file://HOST/tmp/exttab_list') format 'csv';
alter table pt_ext exchange partition part1 with table ret without validation;
drop table ret;
--end_ignore

create table pt_ext_heap(like pt_ext);
alter table pt_ext exchange partition part1 with table pt_ext_heap;
\d pt_ext_1_prt_part1 
drop external table pt_ext_heap;
