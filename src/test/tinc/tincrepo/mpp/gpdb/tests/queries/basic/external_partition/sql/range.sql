-- 
-- @created 2015-07-11 12:00:00
-- @modified 2015-07-11 12:00:00
-- @tags external_partition 
-- @gpdiff true 
-- @description Tests for range partition 

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
partition by range(col2)
(
	partition part1 start ('1')  end ('10'), 
	partition part2 start ('10') end ('20'), 
	partition part3 start ('20') end ('30'), 
	partition part4 start ('30') end ('40'), 
	partition part5 start ('40') end ('50') 
);


insert into pt_ext select i,i,'test',true from generate_series(1,49) i;
create temp table tmp as select * from pt_ext where col1 < 10; 
copy tmp to '/tmp/exttab_range' csv;
create readable external table ret(like pt_ext) location('file://HOST/tmp/exttab_range') format 'csv';
alter table pt_ext exchange partition part1 with table ret without validation;
drop table ret;
--end_ignore

select count(*) from pt_ext;
select count(*) from pt_ext_1_prt_part1;

--start_ignore
drop table pt_ext;
\! rm /tmp/exttab_range
--end_ignore
