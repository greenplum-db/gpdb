-- 
-- @created 2015-07-23 12:00:00
-- @modified 2015-07-23 12:00:00
-- @tags external_partition 
-- @gpdiff true 
-- @check ORCA would fallback when query has external partition

--start_ignore
drop table if exists pt_ext_opt;
CREATE TABLE pt_ext_opt
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

insert into pt_ext_opt select i,i from generate_series(1,6) i;
create temp table tmp as select * from pt_ext_opt where col1 < 4 distributed by (col1); 
copy tmp to '/tmp/exttab_text';
create readable external table ret(like pt_ext_opt) location('file://HOST/tmp/exttab_text') format 'text';
alter table pt_ext_opt exchange partition part1 with table ret without validation;
drop table ret;
--end_ignore

set optimizer = on;
select * from pt_ext_opt;
select * from (select * from pt_ext_opt) foo;
with foo as (select * from pt_ext_opt) select * from foo;
set optimizer = off;

--start_ignore
drop table pt_ext_opt;
\! rm /tmp/exttab_text
--end_ignore
