DROP DATABASE IF EXISTS testdb3;
CREATE DATABASE testdb3 WITH LC_COLLATE='C' LC_CTYPE='C' TEMPLATE=template0;
\c testdb3

CREATE TABLE hi_안녕세계 (a int, 안녕세계 text) DISTRIBUTED BY (a);
INSERT INTO hi_안녕세계 VALUES(1, '안녕세계 1');
INSERT INTO hi_안녕세계 VALUES(42, '안녕세계 42');
SET optimizer_trace_fallback=on;

EXPLAIN DELETE FROM hi_안녕세계 WHERE a=42;
DELETE FROM hi_안녕세계 WHERE a=42;

EXPLAIN SELECT * FROM hi_안녕세계;
SELECT * FROM hi_안녕세계;
