-- default should be 1000
show gp_initial_bad_row_limit;

DROP EXTERNAL TABLE IF EXISTS exttab_first_reject_limit_1 cascade;

-- Generate the file with 5000 error rows at the beginning of the file
\! python @errors@ 10000 5000 > @data_dir@/exttab_first_reject_limit_1.tbl

CREATE EXTERNAL TABLE exttab_first_reject_limit_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_first_reject_limit_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 20000;

-- should fail with an appropriate error message
SELECT COUNT(*) FROM exttab_first_reject_limit_1;

SELECT COUNT(*) > 0 FROM gp_read_error_log('exttab_first_reject_limit_1');


set gp_initial_bad_row_limit = 6000;

-- should work now
select gp_truncate_error_log('exttab_first_reject_limit_1');

SELECT COUNT(*) FROM exttab_first_reject_limit_1;

select COUNT(*) FROM gp_read_error_log('exttab_first_reject_limit_1');

-- first segment reject limit should be checked before segment reject limit

DROP EXTERNAL TABLE IF EXISTS exttab_first_reject_limit_2;

CREATE EXTERNAL TABLE exttab_first_reject_limit_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_first_reject_limit_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 500;

set gp_initial_bad_row_limit = 2;

-- should report an error saying first rows were rejected
SELECT COUNT(*) FROM exttab_first_reject_limit_2;

SELECT COUNT(*) from gp_read_error_log('exttab_first_reject_limit_2');

set gp_initial_bad_row_limit = 600;

-- should report an error saying segment reject limit reached
SELECT gp_truncate_error_log('exttab_first_reject_limit_2');

SELECT COUNT(*) FROM exttab_first_reject_limit_2;

SELECT COUNT(*) from gp_read_error_log('exttab_first_reject_limit_2');


-- set unlimited first error rows, should fail only because of segment reject limits
set gp_initial_bad_row_limit = 0;

SELECT gp_truncate_error_log('exttab_first_reject_limit_2');

SELECT COUNT(*) FROM exttab_first_reject_limit_2;

SELECT COUNT(*) from gp_read_error_log('exttab_first_reject_limit_2');


-- With copy command
DROP TABLE IF EXISTS test_first_segment_reject_limit;

CREATE TABLE test_first_segment_reject_limit (LIKE exttab_first_reject_limit_1);

SET gp_initial_bad_row_limit = 500;

COPY test_first_segment_reject_limit FROM '@data_dir@/exttab_first_reject_limit_1.tbl' WITH DELIMITER '|' segment reject limit 20000;

SET gp_initial_bad_row_limit = 6000;

-- should go through fine
COPY test_first_segment_reject_limit FROM '@data_dir@/exttab_first_reject_limit_1.tbl' WITH DELIMITER '|' segment reject limit 20000;

SELECT COUNT(*) FROM test_first_segment_reject_limit;
