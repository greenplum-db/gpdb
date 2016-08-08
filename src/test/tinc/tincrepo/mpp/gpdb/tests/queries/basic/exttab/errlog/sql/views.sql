DROP EXTERNAL TABLE IF EXISTS exttab_views_1 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_views_2 CASCADE;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_views_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_views_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_views_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_views_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_views_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_views_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Test: views reaching segment reject limit
DROP VIEW IF EXISTS exxttab_views_3;

CREATE VIEW exttab_views_3 as
SELECT sum(distinct e1.i) as sum_i, sum(distinct e2.i) as sum_j, e1.j as j FROM

(SELECT i, j FROM exttab_views_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_views_2 WHERE i < 10) e2

group by e1.j;

SELECT gp_truncate_error_log('exttab_views_1');
SELECT gp_truncate_error_log('exttab_views_2');

-- This should error out
SELECT * FROM exttab_views_3;

-- Error table should be populated
SELECT count(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_views_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_views_2')
) FOO;

-- INSERT INTO FROM a view
DROP TABLE IF EXISTS exttab_views_insert_1;

CREATE TABLE exttab_views_insert_1 (i int, j int, k text);

SELECT gp_truncate_error_log('exttab_views_1');
SELECT gp_truncate_error_log('exttab_views_2');

-- Should fail
INSERT INTO exttab_views_insert_1 SELECT * FROM exttab_views_3;

-- should not have any rows
SELECT * FROM exttab_views_insert_1; 

-- Error table should be populated
SELECT count(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_views_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_views_2')
) FOO;

-- CTAS from a view with segment reject limit reached
DROP TABLE IF EXISTS exttab_views_ctas_1;

SELECT gp_truncate_error_log('exttab_views_1');
SELECT gp_truncate_error_log('exttab_views_2');

-- Should fail 
CREATE TABLE exttab_views_ctas_1 AS SELECT * FROM exttab_views_3 where j < 100;

SELECT * FROM exttab_views_ctas_1;

-- Error table should be populated
SELECT count(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_views_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_views_2')
) FOO;


-- CTAS FROM view with segment reject limits reached
DROP TABLE IF EXISTS exttab_views_ctas_1;

SELECT gp_truncate_error_log('exttab_views_1');
SELECT gp_truncate_error_log('exttab_views_2');

-- Should fail here 
CREATE TABLE exttab_views_ctas_1 AS SELECT * FROM exttab_views_3;

-- Relation should not exist
SELECT * FROM exttab_views_ctas_1;

-- Error table should be populated

SELECT count(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_views_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_views_2')
) FOO;