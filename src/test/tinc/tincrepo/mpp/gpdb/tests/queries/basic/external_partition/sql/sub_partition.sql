-- 
-- @created 2015-07-11 12:00:00
-- @modified 2015-07-11 12:00:00
-- @tags external_partition 
-- @gpdiff true 
-- @description Tests for sub partition 

--start_ignore
drop table if exists pt_ext;
CREATE TABLE pt_ext
(
  col1 int,
  col2 decimal,
  col3 int,
  col4 bool
  
)
distributed by (col1)
partition by range(col2)
SUBPARTITION BY RANGE (col3)
SUBPARTITION TEMPLATE (
SUBPARTITION sub1 START (2001),
SUBPARTITION sub2 START (2002),
SUBPARTITION sub3 START (2006) END (2007) )
(
	partition part1 start ('1')  end ('10'), 
	partition part2 start ('10') end ('20'), 
	partition part3 start ('20') end ('30'), 
	partition part4 start ('30') end ('40'), 
	partition part5 start ('40') end ('50') 
);

create readable external table ret(like pt_ext) location('file://HOST/tmp/exttab_list') format 'csv';
--end_ignore

alter table pt_ext alter partition part1 exchange partition sub1 with table ret without validation;
drop external table ret;
