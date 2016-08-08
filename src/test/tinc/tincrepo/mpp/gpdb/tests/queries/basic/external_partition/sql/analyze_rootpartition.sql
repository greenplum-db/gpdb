-- 
-- @created 2015-07-11 12:00:00
-- @modified 2015-07-11 12:00:00
-- @tags external_partition 
-- @gpdiff true 
-- @description Tests for analyze table 

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
	start ('1') end('100000') every ('20000')
);

insert into pt_ext select i,i,'test',true from generate_series(1,99999) i;
\! rm /tmp/exttab_range1
\! rm /tmp/exttab_range2
\! rm /tmp/exttab_range3
copy (select * from pt_ext where col2 >= 1 and col2 <= 20000) to '/tmp/exttab_range1' csv;
copy (select * from pt_ext where col2 >= 40001 and col2 <= 60000) to '/tmp/exttab_range2' csv;
copy (select * from pt_ext where col2 >= 80001 and col2 <= 99999) to '/tmp/exttab_range3' csv;
create readable external table ret1(like pt_ext) location('file://HOST/tmp/exttab_range1') format 'csv';
create readable external table ret2(like pt_ext) location('file://HOST/tmp/exttab_range2') format 'csv';
create readable external table ret3(like pt_ext) location('file://HOST/tmp/exttab_range3') format 'csv';

alter table pt_ext exchange partition for(1) with table ret1 without validation;
alter table pt_ext exchange partition for(40001) with table ret2 without validation;
alter table pt_ext exchange partition for(80001) with table ret3 without validation;

drop table ret1;
drop table ret2;
drop table ret3;
--end_ignore

analyze rootpartition pt_ext;
select round(reltuples/10000.0) from pg_class where relname='pt_ext';

--start_ignore
drop table pt_ext;
\! rm /tmp/exttab_range1
\! rm /tmp/exttab_range2
\! rm /tmp/exttab_range3
--end_ignore
