-- 
-- @created 2015-07-11 12:00:00
-- @modified 2015-07-11 12:00:00
-- @tags external_partition 
-- @gpdiff true 
-- @description Tests for error table 


--start_ignore
\! rm /tmp/exttab_range
\! rm /tmp/exttab_range1

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
copy(select col1,col2,col3,'abc' from pt_ext where col2 < 10) to '/tmp/exttab_range' csv; 
copy(select col1,col2,col3,'abc' from pt_ext where col2 < 20 and col2 > 10) to '/tmp/exttab_range1' csv; 
create readable external table ret(like pt_ext) location('file://HOST/tmp/exttab_range') format 'csv' LOG ERRORS SEGMENT REJECT LIMIT 20;
create readable external table ret1(like pt_ext) location('file://HOST/tmp/exttab_range1') format 'csv' LOG ERRORS SEGMENT REJECT LIMIT 20;
alter table pt_ext exchange partition part1 with table ret without validation;
alter table pt_ext exchange partition part2 with table ret1 without validation;
drop table ret;
drop table ret1;
--end_ignore

select gp_truncate_error_log('pt_ext_1_prt_part1'); 
select count(*) from pt_ext;
select count(*) from gp_read_error_log('pt_ext_1_prt_part1'); 

--start_ignore
drop table pt_ext;
drop table pt_ext_errtable;
\! rm /tmp/exttab_range
\! rm /tmp/exttab_range1
--end_ignore
