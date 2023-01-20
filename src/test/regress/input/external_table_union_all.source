SET optimizer_trace_fallback=on;
-- Test external table as left child of union all with replicated table
CREATE EXTERNAL TABLE multilocation_external_table(a INTEGER)
location ('file://@hostname@@abs_srcdir@/data/location1.csv', 'file://@hostname@@abs_srcdir@/data/location2.csv', 'file://@hostname@@abs_srcdir@/data/location3.csv')
ON ALL FORMAT 'text';
CREATE EXTERNAL TABLE one_external_table(a INTEGER)
location ('file://@hostname@@abs_srcdir@/data/location2.csv')
ON SEGMENT 2 FORMAT 'text';
CREATE TABLE simple_replicated_table(a integer) DISTRIBUTED REPLICATED;
INSERT INTO simple_replicated_table VALUES (1);
CREATE TABLE simple_distributed_table(a integer) DISTRIBUTED BY (a);
INSERT INTO simple_distributed_table VALUES (2);

EXPLAIN SELECT A FROM multilocation_external_table UNION ALL SELECT A FROM simple_replicated_table;
SELECT A FROM multilocation_external_table UNION ALL SELECT A FROM simple_replicated_table;

EXPLAIN SELECT A FROM simple_replicated_table UNION ALL SELECT A FROM multilocation_external_table;
SELECT A FROM simple_replicated_table UNION ALL SELECT A FROM multilocation_external_table;

EXPLAIN SELECT A FROM simple_replicated_table UNION ALL SELECT a FROM multilocation_external_table UNION ALL SELECT A FROM simple_distributed_table;
SELECT A FROM simple_replicated_table UNION ALL SELECT a FROM multilocation_external_table UNION ALL SELECT A FROM simple_distributed_table;

EXPLAIN SELECT A FROM simple_distributed_table UNION ALL SELECT a FROM multilocation_external_table UNION ALL SELECT A FROM simple_replicated_table;
SELECT A FROM simple_distributed_table UNION ALL SELECT a FROM multilocation_external_table UNION ALL SELECT A FROM simple_replicated_table;

EXPLAIN SELECT * FROM one_external_table UNION ALL SELECT a FROM simple_replicated_table;
SELECT * FROM one_external_table UNION ALL SELECT a FROM simple_replicated_table;
