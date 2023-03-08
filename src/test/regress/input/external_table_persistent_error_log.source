--
-- external tables - short and simple functional tests.
--
-- start_matchsubs
--
-- # replace return code in error message (platform specific)
--
-- m/ERROR\:\s+external table .* command ended with .* not found/
-- s/nosuchcommand\:\s*(command)? not found/nosuchcommand\: NOT FOUND/
--
-- m/ERROR\:\s+external table .* command ended with .*No such file.*/
-- s/nosuchfile\.txt\:\s*No such file (or directory)?/nosuchfile\.txt\: NO SUCH FILE/
-- m/ERROR\:\s+external table .* command ended with .*No such file.*/i
-- s/cat\: (cannot open)? nosuchfile\.txt/cat\: nosuchfile\.txt/
--
-- # remove line number - redhat
-- m/ERROR\:\s+external table .* command ended with .*NOT FOUND.*/i
-- s/\s+line \d+\://
-- # remove line number - Debian
-- m/ERROR\:\s+external table .* command ended with .*sh: 1: .*NOT FOUND.*/i
-- s/ sh: 1: / sh: /
--
-- m/DETAIL:  Found \d+ URLs and \d+ primary segments./
-- s/Found.+//
--
-- end_matchsubs

-- start_ignore
DROP EXTERNAL TABLE IF EXISTS ext_error_persistent;
DROP EXTERNAL TABLE IF EXISTS ext_bytea;
-- end_ignore
SET optimizer_trace_fallback=on;

COPY (VALUES('1,2'),('1,2,3'),('1,'),('1')) TO '@abs_srcdir@/data/tableerr.csv';

create external table ext_error_persistent(a int, b int)
location ('file://@hostname@@abs_srcdir@/data/tableerr.csv')
format 'csv'
log errors persistently segment reject limit 10;

select * from ext_error_persistent;

SELECT (gp_read_persistent_error_log('ext_error_persistent')).errmsg;

select errmsg from gp_read_error_log('ext_error_persistent');

select * from gp_truncate_error_log('ext_error_persistent');

select * from gp_truncate_error_log('*');

select * from gp_truncate_error_log('*.*');

SELECT (gp_read_persistent_error_log('ext_error_persistent')).errmsg;

SELECT gp_truncate_persistent_error_log('ext_error_persistent');

SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');

select * from ext_error_persistent;
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');
SELECT gp_truncate_persistent_error_log('*');
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');


select * from ext_error_persistent;
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');
SELECT gp_truncate_persistent_error_log('*.*');
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');

select * from ext_error_persistent;
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');

DROP EXTERNAL TABLE ext_error_persistent;
-- error log still exists
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');

-- with out the error_log_persistent option, which will use normal error log.
create external table ext_error_persistent(a int, b int)
location ('file://@hostname@@abs_srcdir@/data/tableerr.csv')
format 'csv'
log errors segment reject limit 10;

select * from ext_error_persistent;
SELECT relname, linenum, errmsg FROM gp_read_error_log('ext_error_persistent');
-- persistent error log has no change
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');

DROP EXTERNAL TABLE ext_error_persistent;
SELECT relname, linenum, errmsg FROM gp_read_error_log('ext_error_persistent');
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');

SELECT gp_truncate_persistent_error_log('ext_error_persistent');
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_error_persistent');

CREATE EXTERNAL TABLE ext_bytea (id int, content bytea)
LOCATION (
    'file://@hostname@@abs_srcdir@/data/bytea.data'
) FORMAT 'CSV'
OPTIONS (error_log_persistent 'true')
LOG ERRORS SEGMENT REJECT LIMIT 5;

SELECT * FROM ext_bytea;

SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_bytea');
SELECT gp_truncate_persistent_error_log('ext_bytea');
SELECT relname, linenum, errmsg FROM gp_read_persistent_error_log('ext_bytea');


\! rm @abs_srcdir@/data/tableerr.csv
