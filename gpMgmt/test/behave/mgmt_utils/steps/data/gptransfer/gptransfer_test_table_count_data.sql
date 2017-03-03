DROP DATABASE gptransfer_testdb_table_count_one;
DROP DATABASE gptransfer_testdb_table_count_two;

CREATE DATABASE gptransfer_testdb_table_count_one;
CREATE DATABASE gptransfer_testdb_table_count_two;

\c gptransfer_testdb_table_count_one

CREATE TABLE t1 AS SELECT * FROM generate_series(1,5);

\c gptransfer_testdb_table_count_two

CREATE TABLE table1 (name TEXT, address TEXT);

INSERT INTO table1 VALUES('Harry', 'CA');
INSERT INTO table1 VALUES('Jack', 'NC');

CREATE TABLE table2 AS SELECT * FROM generate_series(5,10);
