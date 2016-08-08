-- 
-- @created 2015-07-23 12:00:00
-- @modified 2015-07-23 12:00:00
-- @tags external_partition 
-- @gpdiff true 
-- @check whether external partition works on gphdfs

drop table if exists pt_ext_web;
CREATE TABLE pt_ext_web
(
  col1 int,
  col2 decimal
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3),
	partition part2 values(4,5,6)
);
insert into pt_ext_web select i,i from generate_series(1,6) i;

create temp table tmp as select * from pt_ext_web where col1 < 4 distributed by (col1); 
copy tmp to '/tmp/pt_ext_web_out';

drop external table if exists pt_ext_web_ext;
create external web table pt_ext_web_ext(like pt_ext_web) execute 'cat /tmp/pt_ext_web_out' on master format 'text';

select * from pt_ext_web_ext;
alter table pt_ext_web exchange partition part1 with table pt_ext_web_ext without validation;
drop table if exists pt_ext_web_ext;

select * from pt_ext_web;

drop table pt_ext_web;
\! rm '/tmp/pt_ext_web_out'
