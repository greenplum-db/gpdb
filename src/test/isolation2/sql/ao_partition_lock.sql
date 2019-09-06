-- Previously, even directly insert into a partition ao
-- table's child partition, the transaction will lock
-- all the tables' aoseg table in this partition on
-- each segment. This test case is to test that extra
-- lock is not acquired.

create table test_ao_partition_lock
( field_dk integer ,field_part integer)
with (appendonly=true)
DISTRIBUTED BY (field_dk)
PARTITION BY LIST(field_part)
(
  partition val1 values(1),
  partition val2 values(2),
  partition val3 values(3)
);

1: begin;
1: insert into test_ao_partition_lock_1_prt_val1 values(1,1);

2: begin;
2: alter table test_ao_partition_lock truncate partition for (2);
2: end;

1: end;

1q:
2q:

drop table test_ao_partition_lock;
