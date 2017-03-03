DROP DATABASE gptransfer_test_db;

CREATE DATABASE gptransfer_test_db;

\c gptransfer_test_db

CREATE TABLE t1 (id int, description text);

INSERT INTO t1 VALUES (2, 'something, anything');
INSERT INTO t1 VALUES (3, 'something
more
to 
describe');
INSERT INTO t1 VALUES (4, 'details: "something_more"');
INSERT INTO t1 VALUES (5, E'something \'special\'');
