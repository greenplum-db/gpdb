DROP EXTERNAL TABLE IF EXISTS exttab_cursor_1;
DROP EXTERNAL TABLE IF EXISTS exttab_cursor_2;

DROP TABLE IF EXISTS exttab_cursor_err;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_cursor_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_cursor_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_cursor_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_cursor_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_cursor_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_cursor_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;


-- Test: Define a cursor on an external table scan query with segment reject limit reached
BEGIN;

DECLARE exttab_cur1 no scroll cursor FOR
SELECT e1.i, e2.j from exttab_cursor_2 e1 INNER JOIN exttab_cursor_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_cursor_2 e1 INNER JOIN exttab_cursor_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_cursor_2 e1 INNER JOIN exttab_cursor_2 e2 ON e1.i = e2.i;

COMMIT;

-- This should have the errors populated already
SELECT count(*) > 0 FROM gp_read_error_log('exttab_cursor_2');

-- Test: Fetch on external table scans with segment reject limit reached
SELECT gp_truncate_error_log('exttab_cursor_1');
SELECT gp_truncate_error_log('exttab_cursor_2');


BEGIN;

DECLARE exttab_cur1 no scroll cursor FOR
SELECT e1.i, e2.j from exttab_cursor_2 e1 INNER JOIN exttab_cursor_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_cursor_2 e1 INNER JOIN exttab_cursor_2 e2 ON e1.i = e2.i
UNION ALL
SELECT e1.i, e2.j from exttab_cursor_2 e1 INNER JOIN exttab_cursor_2 e2 ON e1.i = e2.i;

-- Should not fail as we would not have reached segment reject limit yet. MPP-23814. This fails currently though
FETCH exttab_cur1;

COMMIT;

-- This should have errors populated already
SELECT count(*) > 0 FROM gp_read_error_log('exttab_cursor_2');

-- Test: Fetch on external table scans without reaching segment reject limit
SELECT gp_truncate_error_log('exttab_cursor_1');
SELECT gp_truncate_error_log('exttab_cursor_2');


BEGIN;

DECLARE exttab_cur1 no scroll cursor FOR
SELECT e1.i, e2.j from exttab_cursor_1 e1 INNER JOIN exttab_cursor_1 e2 ON e1.i = e2.i
UNION
SELECT e1.i, e2.j from exttab_cursor_1 e1 INNER JOIN exttab_cursor_1 e2 ON e1.i = e2.i;

-- Should not fail
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;
FETCH exttab_cur1;

COMMIT;

-- This should have errors populated already
SELECT count(*) > 0 FROM gp_read_error_log('exttab_cursor_1');

-- Test: Rollback transaction after populating error logs
SELECT gp_truncate_error_log('exttab_cursor_1');
SELECT gp_truncate_error_log('exttab_cursor_2');


BEGIN;

-- This would have populated the error logs
DECLARE exttab_cur1 no scroll cursor FOR
SELECT e1.i, e2.j from exttab_cursor_1 e1 INNER JOIN exttab_cursor_1 e2 ON e1.i = e2.i
UNION
SELECT e1.i, e2.j from exttab_cursor_1 e1 INNER JOIN exttab_cursor_1 e2 ON e1.i = e2.i;

CREATE TABLE exttab_cursor_foo(i int, j int);

ROLLBACK;

-- Error logs should be populated even if the transaction was rolled back.
SELECT count(*) > 0 FROM gp_read_error_log('exttab_cursor_1');























