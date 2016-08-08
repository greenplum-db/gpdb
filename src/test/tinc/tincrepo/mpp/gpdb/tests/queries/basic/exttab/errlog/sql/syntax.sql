-- Test new syntax for create external table and copy

DROP EXTERNAL TABLE IF EXISTS exttab_syntax_1;
DROP EXTERNAL TABLE IF EXISTS exttab_syntax_2;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_syntax_1.tbl

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_syntax_2.tbl

-- Without error logs or error tables
CREATE EXTERNAL TABLE exttab_syntax_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_syntax_1.tbl') FORMAT 'TEXT' (DELIMITER '|');

SELECT COUNT(*) FROM exttab_syntax_1;

SELECT COUNT(*) FROM gp_read_error_log('exttab_syntax_1');

SELECT gp_truncate_error_log('exttab_syntax_1');

DROP EXTERNAL TABLE IF EXISTS exttab_syntax_1;


-- Error logs with percentage
CREATE EXTERNAL TABLE exttab_syntax_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_syntax_1.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 9 PERCENT;

SELECT COUNT(*) FROM exttab_syntax_1;

SELECT COUNT(*) FROM gp_read_error_log('exttab_syntax_1');

SELECT gp_truncate_error_log('exttab_syntax_1');

DROP EXTERNAL TABLE IF EXISTS exttab_syntax_1;

-- Error logs with percentage, reaching segment reject limit
DROP EXTERNAL TABLE IF EXISTS exttab_syntax_2;

SELECT * FROM gp_read_error_log('exttab_syntax_2');

CREATE EXTERNAL TABLE exttab_syntax_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_syntax_2.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 1 PERCENT;

-- Set gp_reject_percent_threshold to error out after one bad row per segment after processing 100 initial rows
SET gp_reject_percent_threshold = 100;
SELECT COUNT(*) FROM exttab_syntax_2;

SELECT COUNT(*) FROM gp_read_error_log('exttab_syntax_2');

SELECT gp_truncate_error_log('exttab_syntax_2');

-- Try COPY with KEEP. Should error out.
DROP TABLE IF EXISTS exttab_syntax_copy;
CREATE TABLE exttab_syntax_copy (LIKE exttab_syntax_1);
COPY exttab_syntax_copy FROM '@data_dir@/exttab_syntax_2.tbl' LOG ERRORS KEEP SEGMENT REJECT LIMIT 10;

