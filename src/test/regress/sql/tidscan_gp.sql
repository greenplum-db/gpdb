drop table if exists test;
create table test (a integer);
insert into test select a from generate_series(1,100) a;

\echo -- start_ignore
select * from test where ctid='(0,10)' and gp_segment_id=1;
\echo -- end_ignore
drop table if exists test;
create table test (a integer);
insert into test select a from generate_series(1,100) a;

\echo -- start_ignore
select * from test where ctid in ('(0,10)', '(0,20)');
\echo -- end_ignore
