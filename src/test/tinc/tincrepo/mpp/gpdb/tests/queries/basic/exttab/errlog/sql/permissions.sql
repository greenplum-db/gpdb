-- Check permissions with gp_truncate_error_log and gp_read_error_log

DROP EXTERNAL TABLE IF EXISTS exttab_permissions_1;
DROP EXTERNAL TABLE IF EXISTS exttab_permissions_2;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_permissions_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_permissions_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_permissions_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_permissions_2.tbl

-- reaches reject limit
CREATE EXTERNAL TABLE exttab_permissions_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_permissions_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- generate some error logs
SELECT COUNT(*) FROM exttab_permissions_1, exttab_permissions_2;

-- Test: that only superuser can do gp_truncate_error_log('*.*')
DROP ROLE IF EXISTS exttab_non_superuser;

CREATE ROLE exttab_non_superuser WITH NOSUPERUSER LOGIN CREATEDB;

SET ROLE exttab_non_superuser;

SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_2');

SELECT gp_truncate_error_log('exttab_permissions_1');
SELECT gp_truncate_error_log('exttab_permissions_2');
SELECT gp_truncate_error_log('*');
SELECT gp_truncate_error_log('*.*');

SET ROLE @user@;

DROP ROLE IF EXISTS exttab_superuser;

CREATE ROLE exttab_superuser WITH SUPERUSER LOGIN;

SET ROLE exttab_superuser;

SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_1');
SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_2');

SELECT gp_truncate_error_log('*');
SELECT gp_truncate_error_log('*.*');
SELECT gp_truncate_error_log('exttab_permissions_1');
SELECT gp_truncate_error_log('exttab_permissions_2');

SET ROLE @user@;

SELECT * FROM gp_read_error_log('exttab_permissions_1');
SELECT * FROM gp_read_error_log('exttab_permissions_2');

-- Test: only database owner can do gp_truncate_error_log('*')
DROP DATABASE IF EXISTS exttab_db;
DROP ROLE IF EXISTS exttab_user1;
DROP ROLE IF EXISTS exttab_user2;

CREATE ROLE exttab_user1 WITH NOSUPERUSER LOGIN;
CREATE ROLE exttab_user2 WITH NOSUPERUSER LOGIN;


CREATE DATABASE exttab_db WITH OWNER=exttab_user1;

\c exttab_db
-- generate some error logs in this db
DROP EXTERNAL TABLE IF EXISTS exttab_permissions_1 CASCADE;

CREATE EXTERNAL TABLE exttab_permissions_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_permissions_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

SELECT COUNT(*) FROM exttab_permissions_1 e1, exttab_permissions_1 e2;

SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_1');

SET ROLE exttab_user2;
SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_1');
SELECT gp_truncate_error_log('*');
SELECT gp_truncate_error_log('*.*');
SELECT gp_truncate_error_log('exttab_permissions_1');

SET ROLE exttab_user1;

-- Database owner can still not perform read / truncate on specific tables. This follows the same mechanism as TRUNCATE table.
SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_1');
SELECT gp_truncate_error_log('exttab_permissions_1');
SELECT gp_truncate_error_log('*');
-- should fail
SELECT gp_truncate_error_log('*.*');

SET ROLE @user@;

SELECT * FROM gp_read_error_log('exttab_permissions_1');

\c @dbname@
DROP EXTERNAL TABLE IF EXISTS exttab_permissions_3 CASCADE;

DROP ROLE IF EXISTS errlog_exttab_user3;
DROP ROLE IF EXISTS errlog_exttab_user4;

CREATE ROLE errlog_exttab_user3 WITH NOSUPERUSER LOGIN;
CREATE ROLE errlog_exttab_user4 WITH NOSUPERUSER LOGIN;

-- generate some error logs in this db
CREATE EXTERNAL TABLE exttab_permissions_3( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_permissions_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

SELECT COUNT(*) FROM exttab_permissions_3 e1, exttab_permissions_3 e2;

SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_3');

ALTER EXTERNAL TABLE exttab_permissions_3 OWNER TO errlog_exttab_user3;

-- This should fail with non table owner
SET ROLE errlog_exttab_user4;
SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_3');
SELECT gp_truncate_error_log('exttab_permissions_3');

-- should go through fine with table owner
SET ROLE errlog_exttab_user3;
SELECT gp_truncate_error_log('exttab_permissions_3');

SET ROLE @user@;
SELECT * FROM gp_read_error_log('exttab_permissions_3');

-- Grant TRUNCATE permission on table to a non table owner and make sure he is able to do gp_truncate_error_log
GRANT TRUNCATE on exttab_permissions_3 to errlog_exttab_user4;

SELECT COUNT(*) FROM exttab_permissions_3 e1, exttab_permissions_3 e2;
SELECT COUNT(*) FROM gp_read_error_log('exttab_permissions_3');

SET ROLE errlog_exttab_user4;

SELECT gp_truncate_error_log('exttab_permissions_3');

SET ROLE @user@;
SELECT * FROM gp_read_error_log('exttab_permissions_3');

