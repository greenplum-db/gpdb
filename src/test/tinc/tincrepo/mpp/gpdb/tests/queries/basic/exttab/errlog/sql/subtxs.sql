DROP EXTERNAL TABLE IF EXISTS exttab_subtxs_1;
DROP EXTERNAL TABLE IF EXISTS exttab_subtxs_2;

-- Generate the file with very few errors
\! python @script@ 40 5 > @data_dir@/exttab_subtxs_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_subtxs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_subtxs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 300 70 > @data_dir@/exttab_subtxs_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_subtxs_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_subtxs_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Test: TRUNCATE / delete / write to error logs within subtransactions
-- Populate error logs before transaction
SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

BEGIN;

savepoint s1;

SELECT gp_truncate_error_log('exttab_subtxs_1');
SELECT gp_truncate_error_log('exttab_subtxs_2');

SELECT count(*) FROM 
( 
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;


SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

-- should have written rows into error log
SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

savepoint s2;

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

savepoint s3;

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

ROLLBACK TO s2;

-- rollback should not rollback the error rows written from within the transaction
SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

COMMIT;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

-- Test: Segment reject limits reached within subtxs
-- Populate error log before transaction

SELECT gp_truncate_error_log('exttab_subtxs_1');
SELECT gp_truncate_error_log('exttab_subtxs_2');

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

BEGIN;

savepoint s1;

SELECT gp_truncate_error_log('exttab_subtxs_1');
SELECT gp_truncate_error_log('exttab_subtxs_2');

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

-- should have written rows into error log
SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

savepoint s2;

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

savepoint s3;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

ROLLBACK TO s2;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

-- Make the tx fail, segment reject limit reaches here
SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_subtxs_2 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_subtxs_2 WHERE i < 10) e2
WHERE e1.i = e2.i;

COMMIT;

-- Error logs should not have been rolled back. 
SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_subtxs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_subtxs_2')
) FOO;

