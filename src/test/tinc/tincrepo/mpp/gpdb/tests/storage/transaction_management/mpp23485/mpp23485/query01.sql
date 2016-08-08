-- @tag mpp-23485
-- @description shared snapshot files for cursor should be gone after transaction commits.

-- start_ignore
DROP EXTERNAL TABLE IF EXISTS check_cursor_files;
DROP TABLE IF EXISTS cursor_files;
-- end_ignore

CREATE EXTERNAL WEB TABLE check_cursor_files(a int)
EXECUTE 'find base | grep pgsql_tmp | grep _sync | wc -l'
FORMAT 'TEXT';

CREATE TABLE cursor_files(a int, b int);
INSERT INTO cursor_files SELECT i, i FROM generate_series(1, 10)i;
BEGIN;
DECLARE c CURSOR FOR SELECT * FROM cursor_files ORDER BY a;
FETCH 1 FROM c;

-- holdable cursor should be ok
DECLARE c_hold CURSOR WITH HOLD FOR SELECT * FROM cursor_files ORDER BY b;
COMMIT;

SELECT * FROM check_cursor_files WHERE a <> 0;

FETCH 1 FROM c_hold;
