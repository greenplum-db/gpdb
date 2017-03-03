\c gptest;

-- start_ignore
drop table if exists test;
-- end_ignore
create table test (a integer, b integer);
insert into test select a, a%25 from generate_series(1,100) a;

create index test_b on test (b);
