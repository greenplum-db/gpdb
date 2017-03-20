-- @product_version gpdb: [4.3.3-]
-- Some errors without exceeding reject limit
-- CK_SYNC1: TABLE 1
DROP EXTERNAL TABLE IF EXISTS ck_sync1_exttab_error_log_1 cascade;

CREATE EXTERNAL TABLE ck_sync1_exttab_error_log_1( i int, j text )
LOCATION ('gpfdist://10.0.0.6:8088/read/table_with_errors.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 50;

-- CK_SYNC1: TABLE 2
DROP EXTERNAL TABLE IF EXISTS ck_sync1_exttab_error_log_2 cascade;

CREATE EXTERNAL TABLE ck_sync1_exttab_error_log_2( i int, j text )
LOCATION ('gpfdist://10.0.0.6:8088/read/table_with_errors.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 50;

-- CK_SYNC1: TABLE 3
DROP EXTERNAL TABLE IF EXISTS ck_sync1_exttab_error_log_3 cascade;

CREATE EXTERNAL TABLE ck_sync1_exttab_error_log_3( i int, j text )
LOCATION ('gpfdist://10.0.0.6:8088/read/table_with_errors.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 50;

-- CK_SYNC1: TABLE 4
DROP EXTERNAL TABLE IF EXISTS ck_sync1_exttab_error_log_4 cascade;

CREATE EXTERNAL TABLE ck_sync1_exttab_error_log_4( i int, j text )
LOCATION ('gpfdist://10.0.0.6:8088/read/table_with_errors.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 50;

-- CK_SYNC1: TABLE 5
DROP EXTERNAL TABLE IF EXISTS ck_sync1_exttab_error_log_5 cascade;

CREATE EXTERNAL TABLE ck_sync1_exttab_error_log_5( i int, j text )
LOCATION ('gpfdist://10.0.0.6:8088/read/table_with_errors.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 50;

-- CK_SYNC1: TABLE 6
DROP EXTERNAL TABLE IF EXISTS ck_sync1_exttab_error_log_6 cascade;

CREATE EXTERNAL TABLE ck_sync1_exttab_error_log_6( i int, j text )
LOCATION ('gpfdist://10.0.0.6:8088/read/table_with_errors.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 50;

-- CK_SYNC1: TABLE 7
DROP EXTERNAL TABLE IF EXISTS ck_sync1_exttab_error_log_7 cascade;

CREATE EXTERNAL TABLE ck_sync1_exttab_error_log_7( i int, j text )
LOCATION ('gpfdist://10.0.0.6:8088/read/table_with_errors.tbl') FORMAT 'TEXT' (DELIMITER '|')
LOG ERRORS SEGMENT REJECT LIMIT 50;


-- Generate error logs on tables created in SYNC1
SELECT gp_truncate_error_log('sync1_exttab_error_log_2');
-- should not error out as segment reject limit will not be reached
SELECT COUNT(*) FROM sync1_exttab_error_log_2;

-- Error rows logged
SELECT count(*) from gp_read_error_log('sync1_exttab_error_log_2');

SELECT gp_truncate_error_log('sync1_exttab_error_log_2');


-- Generate error logs on tables created in CK_SYNC1
SELECT gp_truncate_error_log('ck_sync1_exttab_error_log_1');
-- should not error out as segment reject limit will not be reached
SELECT COUNT(*) FROM ck_sync1_exttab_error_log_1;

-- Error rows logged
SELECT count(*) from gp_read_error_log('ck_sync1_exttab_error_log_1');

SELECT gp_truncate_error_log('ck_sync1_exttab_error_log_1');
