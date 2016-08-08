DROP EXTERNAL TABLE IF EXISTS exttab_txs_1;
DROP EXTERNAL TABLE IF EXISTS exttab_txs_2;

-- Generate the file with very few errors
\! python @script@ 15 3 > @data_dir@/exttab_txs_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_txs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_txs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 300 70 > @data_dir@/exttab_txs_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_txs_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_txs_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Truncate error logs within transactions
-- Populate error log before transaction
SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_txs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_txs_2')
) FOO;

BEGIN;

SELECT gp_truncate_error_log('exttab_txs_1');
SELECT gp_truncate_error_log('exttab_txs_2');

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

COMMIT;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_txs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_txs_2')
) FOO;

-- Test: TRUNCATE error logs within tx , abort transaction
-- Populate error log before transaction
SELECT gp_truncate_error_log('exttab_txs_1');
SELECT gp_truncate_error_log('exttab_txs_2');

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_txs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_txs_2')
) FOO;

BEGIN;

SELECT gp_truncate_error_log('exttab_txs_1');
SELECT gp_truncate_error_log('exttab_txs_2');

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_txs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_txs_2')
) FOO;

ABORT;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_txs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_txs_2')
) FOO;

-- Test: TRUNCATE error logs within txs , with segment reject limit reached
-- Populate error log before transaction
SELECT gp_truncate_error_log('exttab_txs_1');
SELECT gp_truncate_error_log('exttab_txs_2');

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_1 WHERE i < 10) e2
WHERE e1.i = e2.i;

SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_txs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_txs_2')
) FOO;


BEGIN;

SELECT gp_truncate_error_log('exttab_txs_1');
SELECT gp_truncate_error_log('exttab_txs_2');

-- This should abort the transaction
SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_1 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_2 WHERE i < 10) e2
WHERE e1.i = e2.i;

COMMIT;

-- Additional error rows should have been inserted into the error logs even if the tx is aborted.
-- Truncate of error logs should not be rolled back even if the transaction is aborted. All operation on error logs are persisted. 
SELECT count(*) FROM
(
SELECT * FROM gp_read_error_log('exttab_txs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_txs_2')
) FOO;


-- Test: Creating external table with error log within txs with segment reject limits reached
SELECT gp_truncate_error_log('exttab_txs_1');
SELECT gp_truncate_error_log('exttab_txs_2');

DROP EXTERNAL TABLE IF EXISTS exttab_txs_3;
DROP EXTERNAL TABLE IF EXISTS exttab_txs_4;

BEGIN;

-- create an external table that will reach segment reject limit
-- reaches reject limit
CREATE EXTERNAL TABLE exttab_txs_3( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_txs_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- new error log, within segment reject limit
CREATE EXTERNAL TABLE exttab_txs_4( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_txs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_4 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_4 WHERE i < 10) e2
WHERE e1.i = e2.i order by e1.i;

-- should be populated correctly
SELECT count(*) FROM gp_read_error_log('exttab_txs_4');

-- should error out and abort the transaction
SELECT e1.i, e2.j FROM
(SELECT i, j FROM exttab_txs_3 WHERE i < 5 ) e1,
(SELECT i, j FROM exttab_txs_4 WHERE i < 10) e2
WHERE e1.i = e2.i order by e1.i;

COMMIT;

-- Error logs should not exist for these tables that would have been rolled back 
SELECT count(*) FROM gp_read_error_log('exttab_txs_3');
SELECT count(*) FROM gp_read_error_log('exttab_txs_4');

-- external tables created within aborted transactions should not exist
SELECT count(*) FROM exttab_txs_3;
SELECT count(*) FROM exttab_txs_4;







