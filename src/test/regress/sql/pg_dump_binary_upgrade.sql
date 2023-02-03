-- Ensure that pg_dump --binary-upgrade correctly outputs
-- ALTER TABLE DROP COLUMN DDL for external tables.

CREATE SCHEMA dump_this_schema;

-- External tables with dropped columns is dumped correctly.
CREATE EXTERNAL TABLE dump_this_schema.external_table_with_dropped_columns (
    a int,
    b int,
    c int
) LOCATION ('gpfdist://1.1.1.1:8082/xxxx.csv') FORMAT 'csv';

ALTER EXTERNAL TABLE dump_this_schema.external_table_with_dropped_columns DROP COLUMN a;
ALTER EXTERNAL TABLE dump_this_schema.external_table_with_dropped_columns DROP COLUMN b;

-- Run pg_dump and expect to see an ALTER TABLE DROP COLUMN output
-- for the tables with dropped column references.
\! pg_dump --binary-upgrade --schema dump_this_schema regression | grep " DROP COLUMN "

DROP SCHEMA dump_this_schema CASCADE;
