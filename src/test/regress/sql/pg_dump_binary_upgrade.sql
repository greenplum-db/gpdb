-- Ensure that pg_dump --binary-upgrade correctly suppresses the ALTER
-- TABLE DROP COLUMN DDL ouptut when a GPDB partition table with a
-- dropped column reference on the root partition exists.

CREATE SCHEMA dump_this_schema;
CREATE TABLE dump_this_schema.dropped_column_partition_table_binary_upgrade (
    a int,
    b int,
    c char,
    d varchar(50)
) DISTRIBUTED BY (c)
PARTITION BY RANGE (a)
(
    PARTITION p1 START(1) END(5),
    PARTITION p2 START(5)
);

ALTER TABLE dump_this_schema.dropped_column_partition_table_binary_upgrade DROP COLUMN d;

\! pg_dump --binary-upgrade --schema dump_this_schema regression | grep " DROP COLUMN "

DROP SCHEMA dump_this_schema CASCADE;
