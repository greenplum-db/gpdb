-- Test function gp_read_error_log on non existing table

SELECT gp_read_error_log('non_existing');

-- Test gp_read_error_log on non external table
DROP TABLE IF EXISTS heap_test;
CREATE TABLE heap_test(i int, j int);
SELECT gp_read_error_log('heap_test');

-- Test gp_read_error_log on external table configured with error table
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_1;
DROP TABLE IF EXISTS exttab_funcs_err;

\! python @script@ 10 2 > @data_dir@/exttab_funcs_1.tbl

CREATE EXTERNAL TABLE exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO exttab_funcs_err SEGMENT REJECT LIMIT 10;

SELECT COUNT(*) FROM exttab_funcs_1;

SELECT gp_read_error_log('exttab_funcs_1');

SELECT COUNT(*) FROM exttab_funcs_err;

-- Test gp_read_error_log from a different schema
DROP SCHEMA IF EXISTS exttab_funcs_schema CASCADE;

CREATE SCHEMA exttab_funcs_schema;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_1;

SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');


-- CTAS with gp_read_error_log
DROP TABLE IF EXISTS error_log_ctas;

CREATE TABLE error_log_ctas AS
SELECT relname, linenum, rawdata FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');

SELECT * FROM error_log_ctas order by linenum;

-- INSERT INTO from gp_read_error_log
INSERT INTO error_log_ctas SELECT relname, linenum, rawdata FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');

SELECT * FROM error_log_ctas order by linenum;

-- Test function gp_truncate_error_log on non existing table

SELECT gp_truncate_error_log('non_existing');

-- Test gp_truncate_error_log on non external table
DROP TABLE IF EXISTS heap_test;
CREATE TABLE heap_test(i int, j int);
SELECT gp_truncate_error_log('heap_test');

-- Test gp_truncate_error_log on external table configured with error table
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_1;
DROP TABLE IF EXISTS exttab_funcs_err;

\! python @script@ 10 2 > @data_dir@/exttab_funcs_1.tbl

CREATE EXTERNAL TABLE exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO exttab_funcs_err SEGMENT REJECT LIMIT 10;

SELECT COUNT(*) FROM exttab_funcs_1;

SELECT gp_truncate_error_log('exttab_funcs_1');

SELECT COUNT(*) FROM exttab_funcs_err;

-- Test gp_read_error_log from a different schema
DROP SCHEMA IF EXISTS exttab_funcs_schema CASCADE;

CREATE SCHEMA exttab_funcs_schema;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_1;

SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');

SELECT gp_truncate_error_log('exttab_funcs_schema.exttab_funcs_1');

SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');


-- Test gp_truncate_error_log on all tables
-- Generate error logs
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_1 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_2 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_3 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_schema.exttab_funcs_1 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_schema.exttab_funcs_2 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_schema.exttab_funcs_3 CASCADE;

CREATE EXTERNAL TABLE exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_3( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_3( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

SELECT COUNT(*) FROM exttab_funcs_1;
SELECT COUNT(*) FROM exttab_funcs_2;
SELECT COUNT(*) FROM exttab_funcs_3;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_1;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_2;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_3;

SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_3');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_3');

-- Truncate all log files
SELECT gp_truncate_error_log('*');

SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_3');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_3');

-- Test gp_truncate_error_log('*.*')
-- Generate error logs in the current database
SELECT COUNT(*) FROM exttab_funcs_1;
SELECT COUNT(*) FROM exttab_funcs_2;
SELECT COUNT(*) FROM exttab_funcs_3;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_1;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_2;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_3;

-- Make sure error logs are generated
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_3');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_3');

-- Create and generate error logs in another database
drop database if exists exttab_test;

create database exttab_test;
\c exttab_test

DROP SCHEMA IF EXISTS exttab_funcs_schema CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_1 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_2 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_funcs_3 CASCADE;

CREATE EXTERNAL TABLE exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_3( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE SCHEMA exttab_funcs_schema;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

CREATE EXTERNAL TABLE exttab_funcs_schema.exttab_funcs_3( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_funcs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate error logs in the current database
SELECT COUNT(*) FROM exttab_funcs_1;
SELECT COUNT(*) FROM exttab_funcs_2;
SELECT COUNT(*) FROM exttab_funcs_3;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_1;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_2;
SELECT COUNT(*) FROM exttab_funcs_schema.exttab_funcs_3;

-- Make sure error logs are generated
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_3');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_3');

-- Truncate error logs on all databases 
SELECT gp_truncate_error_log('*.*');

SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_3');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_3');

\c @dbname@
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_3');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_2');
SELECT COUNT(*) FROM gp_read_error_log('exttab_funcs_schema.exttab_funcs_3');
