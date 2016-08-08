-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- start_ignore
drop table if exists test;
-- end_ignore
create table test (a integer, b integer);
insert into test select a, a%25 from generate_series(1,100) a;

create index test_b on test (b);

set enable_seqscan=off;
set enable_bitmapscan=off;
set enable_indexscan=on;

-- start_ignore
explain analyze select a, b from test where b=10 order by a desc;
-- end_ignore

select a, b from test where b=10 order by a desc;
