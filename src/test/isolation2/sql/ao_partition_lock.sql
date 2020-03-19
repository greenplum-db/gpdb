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

-- The following test cases is from a real bug.
-- Previously, user wants to concurrently do the
-- job (each job is a transaction):
--   1. truncate a leaf partition
--   2. insert some new data this leaf partition
--   3. analyze this leaf partition
-- These jobs should be running on different leaf
-- partitions.
CREATE TABLE test_partition
(field_dk bigint,
field_part date,
field_attr text
)
DISTRIBUTED BY (field_dk)
PARTITION BY RANGE(field_part)
(
  PARTITION  p201902 START ('2019-02-01'::date) END ('2019-03-01'::date) WITH (orientation=row, appendonly=true),
  PARTITION  p201903 START ('2019-03-01'::date) END ('2019-04-01'::date) WITH (orientation=row, appendonly=true),
  PARTITION  p201904 START ('2019-04-01'::date) END ('2019-05-01'::date) WITH (orientation=row, appendonly=true)

);

insert into test_partition select id, '2019-02-01'::date + interval '1 days' * mod (id,30), '0' from generate_series(1,123) id;

-- set following GUC to avoid analyze the root partition
-- during insert into a leaf partition.
1: set optimizer_analyze_enable_merge_of_leaf_stats=off;

2: set optimizer_analyze_enable_merge_of_leaf_stats=off;

1: begin;
1: truncate test_partition_1_prt_p201902;

2: begin;
2: truncate test_partition_1_prt_p201903;

-- The following insert statement should only
-- hold locks on their own  leaf partitions so
-- that they could run concurrently.
1: insert into test_partition_1_prt_p201902 select id, '2019-02-01'::date, 0 from generate_series(1, 100)id;
2: insert into test_partition_1_prt_p201903 select id, '2019-03-01'::date, 0 from generate_series(1, 100)id;

1: analyze test_partition_1_prt_p201902;
2: analyze test_partition_1_prt_p201903;

1: end;
2: end;

drop table test_partition;
