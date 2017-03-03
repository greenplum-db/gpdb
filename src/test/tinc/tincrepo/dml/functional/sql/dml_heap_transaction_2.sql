-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Insert on heap tables within transaction 
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_heap;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_heap;
    CREATE TABLE dml_heap (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
    INSERT INTO dml_heap values (7,'to be exchanged','s','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
SAVEPOINT sp2;
    UPDATE dml_heap SET x = 'update';
RELEASE SAVEPOINT sp2;
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_heap;
