-- Test abort transaction should invalidate reader gang's cat cache
-- Discussion: https://groups.google.com/a/greenplum.org/g/gpdb-dev/c/u3-D7isdvmM

set optimizer_force_multistage_agg = 1;

create table dml_14027_union_s (a int not null, b numeric default 10.00) distributed by (a) partition by range(b);
create table dml_14027_union_s_1_prt_2 partition of dml_14027_union_s for values from (1) to (1001);
create table dml_14027_union_s_1_prt_def partition of dml_14027_union_s default;

insert into dml_14027_union_s select generate_series(1,1), generate_series(1,1);

begin;
drop table dml_14027_union_s_1_prt_def;
explain select count(distinct(b)) from dml_14027_union_s;
select count(distinct(b)) from dml_14027_union_s;
rollback;

explain update dml_14027_union_s set a = (select null union select null)::numeric;
-- Should not raise error due to stale catcache in reader gang.
-- eg: ERROR: expected partdefid 134733, but got 0
update dml_14027_union_s set a = (select null union select null)::numeric;

drop table dml_14027_union_s;

reset optimizer_force_multistage_agg;
