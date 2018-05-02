-- Test 1: scan with no errors
DROP EXTERNAL TABLE IF EXISTS exttab_union_1 cascade;

-- Generate the file
\! python @script@ 10 0 > @data_dir@/exttab_union_1.tbl

CREATE EXTERNAL TABLE exttab_union_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Empty error log 
SELECT * FROM gp_read_error_log('exttab_union_1'); 

SELECT COUNT(*) FROM exttab_union_1;

-- Error log should still be empty
SELECT * FROM gp_read_error_log('exttab_union_1');

-- Test 2: Some errors without exceeding reject limit
DROP EXTERNAL TABLE IF EXISTS exttab_union_2 cascade;

-- Generate file with 2 errors
\! python @script@ 10 2 > @data_dir@/exttab_union_2.tbl

CREATE EXTERNAL TABLE exttab_union_2( i int, j text )
LOCATION ('gpfdist://@host@:@port@/exttab_union_2.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- should not error out as segment reject limit will not be reached
SELECT * FROM exttab_union_2 order by i;

-- Error rows logged
SELECT count(*) from gp_read_error_log('exttab_union_2');

-- Test 3: Errors with exceeding reject limit
DROP EXTERNAL TABLE IF EXISTS exttab_basic_3 cascade;

-- Generate file with 10 errors
\! python @script@ 20 10 > @data_dir@/exttab_basic_3.tbl

CREATE EXTERNAL TABLE exttab_basic_3( i int, j text )
LOCATION ('gpfdist://@host@:@port@/exttab_basic_3.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 2;


-- should error out as segment reject limit will be reached
-- Note, this test might not work if there are more than 5 segments in which case we might not reach the limit
SELECT * FROM exttab_basic_3;

-- Error log should have one row that was rejected until the segment reject limit reached
select count(*) from gp_read_error_log('exttab_basic_3');

-- Test 4: Insert into another table
DROP EXTERNAL TABLE IF EXISTS exttab_basic_4 cascade;
DROP EXTERNAL TABLE IF EXISTS exttab_basic_5 cascade;

DROP TABLE IF EXISTS exttab_insert_1;

-- Generate file with 5 errors
\! python @script@ 20 5 > @data_dir@/exttab_basic_4.tbl

CREATE EXTERNAL TABLE exttab_basic_4( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_basic_4.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 100;

CREATE TABLE exttab_insert_1 (LIKE exttab_basic_4);

-- Insert should go through fine
INSERT INTO exttab_insert_1 SELECT * FROM exttab_basic_4;

SELECT * FROM exttab_insert_1 order by i;

-- Error log should have five rows that were rejected
select count(*) from gp_read_error_log('exttab_basic_4');

DROP EXTERNAL TABLE IF EXISTS exttab_basic_5;

-- Generate file with a lot of errors
\! python @script@ 200 100 > @data_dir@/exttab_basic_5.tbl

-- Use the same error log above
CREATE EXTERNAL TABLE exttab_basic_5( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_basic_5.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 5;

-- Insert should fail
INSERT INTO exttab_insert_1 select * from exttab_basic_5;

SELECT * from exttab_insert_1 order by i;

-- Error log should have additional rows that were rejected by the above query
SELECT count(*) from gp_read_error_log('exttab_basic_5');


-- Test 5: CTAS
DROP EXTERNAL TABLE IF EXISTS exttab_basic_6 cascade;
DROP EXTERNAL TABLE IF EXISTS exttab_basic_7 cascade;

DROP TABLE IF EXISTS exttab_ctas_1 cascade;
DROP TABLE IF EXISTS exttab_ctas_2 cascade;

-- Generate file with 5 errors
\! python @script@ 20 5 > @data_dir@/exttab_basic_6.tbl

CREATE EXTERNAL TABLE exttab_basic_6( i int, j text )
LOCATION ('gpfdist://@host@:@port@/exttab_basic_6.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 100;


CREATE TABLE exttab_ctas_1 as SELECT * FROM exttab_basic_6;

-- CTAS should go through fine
SELECT * FROM exttab_ctas_1 order by i;

-- Error log should have five rows that were rejected
select count(*) from gp_read_error_log('exttab_basic_6');

DROP EXTERNAL TABLE IF EXISTS exttab_basic_7;

-- Generate file with a lot of errors
\! python @script@ 200 100 > @data_dir@/exttab_basic_7.tbl

CREATE EXTERNAL TABLE exttab_basic_7( i int, j text )
LOCATION ('gpfdist://@host@:@port@/exttab_basic_7.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 5;

-- CTAS should fail
CREATE TABLE exttab_ctas_2 AS select * from exttab_basic_7;

-- Table should not exist
SELECT * from exttab_ctas_2 order by i;

-- Error table should have additional rows that were rejected by the above query
SELECT count(*) from gp_read_error_log('exttab_basic_7');

-- Test that drop external table gets rid off error logs
DROP EXTERNAL TABLE IF EXISTS exttab_error_log;

CREATE EXTERNAL TABLE exttab_error_log( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

SELECT COUNT(*) FROM exttab_error_log;

SELECT COUNT(*) FROM gp_read_error_log('exttab_error_log');

DROP EXTERNAL TABLE exttab_error_log;

SELECT COUNT(*) FROM gp_read_error_log('exttab_error_log');
