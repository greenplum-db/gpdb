set enable_seqscan=off;
EXPLAIN SELECT * FROM table_with_unique_constraint_renamed WHERE a = 1;
SELECT * FROM table_with_unique_constraint_renamed WHERE a = 1;
-- insert should error out due to unique constraint
INSERT INTO table_with_unique_constraint_renamed VALUES (1,1);

EXPLAIN SELECT * FROM table_with_pk_renamed WHERE a = 1;
SELECT * FROM table_with_pk_renamed WHERE a = 1;
-- insert should error out due to primary key constraint
INSERT INTO table_with_pk_renamed VALUES (1,1);

-- for partitioned table indexes were already dropped
SELECT * FROM part_table_with_pk_renamed;
