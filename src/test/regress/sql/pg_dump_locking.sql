-- Ensure that pg_dump only locks partition tables that are included in the backup set.
-- An existing ACCESS EXCLUSIVE lock on a partition table outside the backup set should
-- not block pg_dump execution.

CREATE SCHEMA dump_this_schema;
CREATE SCHEMA locked_table_schema;

CREATE TABLE dump_this_schema.dump_this_table (
    a int,
    b char,
    c varchar(50)
) DISTRIBUTED BY (b)
PARTITION BY RANGE (a)
(
    PARTITION p1 START(1) END(5),
    PARTITION p2 START(5)
);

CREATE TABLE locked_table_schema.locked_table (
    a int,
    b char,
    c varchar(50)
) DISTRIBUTED BY (b)
PARTITION BY RANGE (a)
(
    PARTITION p1 START(1) END(5),
    PARTITION p2 START(5)
);

BEGIN; LOCK TABLE locked_table_schema.locked_table IN ACCESS EXCLUSIVE MODE;
-- Run pg_dump with the Access Exclusive lock held. We expect pg_dump to complete
-- and output a CREATE TABLE statement for dump_this_schema.dump_this_table.
\! pg_dump -s -t dump_this_schema.dump_this_table regression | grep 'CREATE TABLE'

END;

DROP SCHEMA dump_this_schema CASCADE;
DROP SCHEMA locked_table_schema CASCADE;
