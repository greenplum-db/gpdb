drop table if exists t1;
create table t1(a int, b int);
create index it1 on t1(a) where a < 100;
create index it2 on t1(a) where a != 100;
create index it3 on t1(a, b) where a <= 100 and b >= 100;
select indpred from pg_index where indrelid = 't1'::regclass;
select indpred from gp_dist_random('pg_index') where indrelid = 't1'::regclass;

drop table if exists ao_t1;
create table ao_t1(a int, b int) with(appendonly=true);
create index i_aot1 on ao_t1(a) where a < 100;
create index i_aot2 on ao_t1(a) where a != 100;
create index i_aot3 on ao_t1(a, b) where a <= 100 and b >= 100;
select indpred from pg_index where indrelid = 'ao_t1'::regclass;
select indpred from gp_dist_random('pg_index') where indrelid = 'ao_t1'::regclass;

drop table if exists co_t1;
create table co_t1(a int, b int) with(appendonly=true);
create index i_cot1 on co_t1(a) where a < 100;
create index i_cot2 on co_t1(a) where a != 100;
create index i_cot3 on co_t1(a, b) where a <= 100 and b >= 100;
select indpred from pg_index where indrelid = 'co_t1'::regclass;
select indpred from gp_dist_random('pg_index') where indrelid = 'co_t1'::regclass;

create or replace function myfunc(integer) returns boolean as
$$
select $1 > 20;
$$ language sql immutable;

drop table if exists ao_t2;
create table ao_t2(a int, b int) with (appendonly=true);
create index i_aot4 on ao_t2(b) where myfunc(b);
select count(foo.*) from
 ((select indpred from pg_index where indrelid = 'ao_t2'::regclass)
   union
  (select indpred from gp_dist_random('pg_index') where indrelid = 'ao_t2'::regclass)
 ) foo;
