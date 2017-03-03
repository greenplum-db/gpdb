DROP DATABASE gptransfer_test_db_one;

CREATE DATABASE gptransfer_test_db_one;

\c gptransfer_test_db_one

CREATE TABLE table1 (name TEXT, address TEXT);

INSERT INTO table1 VALUES('Harry', 'California');
INSERT INTO table1 VALUES('Jass', '');
INSERT INTO table1 VALUES('Jack', NULL);
INSERT INTO table1 VALUES('Jill', '    ');
