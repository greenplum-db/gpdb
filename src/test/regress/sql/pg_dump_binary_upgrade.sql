-- 1) Ensure that pg_dump --binary-upgrade correctly outputs
-- ALTER TABLE DROP COLUMN DDL for external tables.

-- 2) Ensure that pg_dump --binary-upgrade correctly suppresses the ALTER
-- TABLE DROP COLUMN DDL ouptut when a GPDB partition table with a
-- dropped column reference on the root partition exists whereas the
-- same dropped column reference does not exist on all of its child
-- partitions. If the same dropped column reference exists on its
-- child partitions, the ALTER TABLE DROP COLUMN DDL should be
-- outputted. Refer to function check_heterogeneous_partition() in
-- pg_upgrade's check_gp.c for more details on homogeneous and
-- heterogeneous partitions.

CREATE SCHEMA dump_this_schema;

-- Scenario 1a: External tables with dropped columns is dumped correctly.
CREATE EXTERNAL TABLE dump_this_schema.external_table_with_dropped_columns_does_not_print_alter_ddl (
    a int,
    b int,
    c int
) LOCATION ('gpfdist://1.1.1.1:8082/xxxx.csv') FORMAT 'csv';

ALTER EXTERNAL TABLE dump_this_schema.external_table_with_dropped_columns DROP COLUMN a;
ALTER EXTERNAL TABLE dump_this_schema.external_table_with_dropped_columns DROP COLUMN b;

-- 2) Scenario 1: Create homogeneous partition table where the root
-- partition and ALL of its child partitions have the dropped column
-- reference.
CREATE TABLE dump_this_schema.dropped_column_homogeneous_partition_table (
    a int,
    b char,
    c varchar(50)
) DISTRIBUTED BY (b)
PARTITION BY RANGE (a)
(
    PARTITION p1 START(1) END(5),
    PARTITION p2 START(5)
);

ALTER TABLE dump_this_schema.dropped_column_homogeneous_partition_table DROP COLUMN c;

-- All of the partition hierarchy members have the same dropped column reference
SELECT relname, relnatts FROM pg_class WHERE relname LIKE 'dropped_column_homogeneous_partition_table%';

SELECT c.relname, a.attname
FROM pg_class c JOIN pg_attribute a ON c.oid = a.attrelid
WHERE a.attisdropped = true AND relname LIKE 'dropped_column_homogeneous_partition_table%';

-- 2) Scenario 2: Create homogeneous partition table where only the root
-- partition has the dropped column reference (as defined by
-- pg_upgrade heterogeneous partition check function).
CREATE TABLE dump_this_schema.dropped_column_special_homogeneous_partition_table (
    a int,
    b char,
    c varchar(50)
) DISTRIBUTED BY (b)
PARTITION BY RANGE (a)
(
    PARTITION p1 START(1) END(5)
);

ALTER TABLE dump_this_schema.dropped_column_special_homogeneous_partition_table DROP COLUMN c;
CREATE TABLE dump_this_schema.staging_table_for_exchange_partition
       AS SELECT * FROM dump_this_schema.dropped_column_special_homogeneous_partition_table_1_prt_p1 DISTRIBUTED BY (b);
ALTER TABLE dump_this_schema.dropped_column_special_homogeneous_partition_table
      EXCHANGE PARTITION p1 WITH TABLE dump_this_schema.staging_table_for_exchange_partition;
DROP TABLE dump_this_schema.staging_table_for_exchange_partition;

-- All of the partition hierarchy members DO NOT have the same dropped column reference
SELECT relname, relnatts FROM pg_class WHERE relname LIKE 'dropped_column_special_homogeneous_partition_table%';

SELECT c.relname, a.attname
FROM pg_class c JOIN pg_attribute a ON c.oid = a.attrelid
WHERE a.attisdropped = true AND relname LIKE 'dropped_column_special_homogeneous_partition_table%';

-- 2) Scenario 3: Create homogeneous partition table where only the root
-- and subroot partition has the dropped column reference (as defined
-- by pg_upgrade heterogeneous partition check function).
CREATE TABLE dump_this_schema.dropped_column_special_homogeneous_partition_with_subpart (
    a int,
    b char,
    c varchar(50)
) DISTRIBUTED BY (b)
PARTITION BY RANGE (a)
SUBPARTITION BY RANGE (b)
(
    PARTITION sp1 START(1) END(5)
    (
	SUBPARTITION p1 START(1) END(3)
    )
);

ALTER TABLE dump_this_schema.dropped_column_special_homogeneous_partition_with_subpart DROP COLUMN c;
CREATE TABLE dump_this_schema.staging_table_for_exchange_partition
       AS SELECT * FROM dump_this_schema.dropped_column_special_homogeneous_partition_with_sub_1_prt_sp1 DISTRIBUTED BY (b);
ALTER TABLE dump_this_schema.dropped_column_special_homogeneous_partition_with_subpart
      ALTER PARTITION sp1 EXCHANGE PARTITION p1 WITH TABLE dump_this_schema.staging_table_for_exchange_partition;
DROP TABLE dump_this_schema.staging_table_for_exchange_partition;

-- All of the partition hierarchy members DO NOT have the same dropped column reference
SELECT relname, relnatts FROM pg_class WHERE relname LIKE 'dropped_column_special_homogeneous_partition_with_subpart%';

SELECT c.relname, a.attname
FROM pg_class c JOIN pg_attribute a ON c.oid = a.attrelid
WHERE a.attisdropped = true AND relname LIKE 'dropped_column_special_homogeneous_partition_with_subpart%';

-- Run pg_dump and expect to see an ALTER TABLE DROP COLUMN output
-- only for the homogeneous partition table where the entire partition
-- table has the same dropped column reference.
-- We do not expect to see ALTER TABLE DROP COLUMN output for external tables
-- because we do not preserve drop columns for external tables.
\! pg_dump --binary-upgrade --schema dump_this_schema regression | grep ' DROP COLUMN \| SET DEFAULT '

DROP SCHEMA dump_this_schema CASCADE;
