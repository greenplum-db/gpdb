-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop table if exists test_fastseqence;
create table test_fastseqence ( a int, b char(20)) with (appendonly = true, orientation=column);
create index test_fastseqence_idx on test_fastseqence(b);
insert into test_fastseqence select i , 'aa'||i from generate_series(1,100) i;
