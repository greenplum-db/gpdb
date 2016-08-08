set enable_partition_fast_insert = on;
set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

-- Test Partition with SREH and hash partition
CREATE TABLE sreh_copy(a int, b int, c int) distributed by(a)
partition by hash(a) partitions 4;

-- 
-- ROW reject limit only
--
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data1.data' DELIMITER '|' SEGMENT REJECT LIMIT 1000;
SELECT * FROM sreh_copy ORDER BY a,b,c;

-- 
-- ROW reject limit only - low value that gets reached
--
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data1.data' DELIMITER '|' SEGMENT REJECT LIMIT 2;
SELECT * FROM sreh_copy ORDER BY a,b,c;

-- 
-- error table (also testing that it gets automatically generated)
--
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data1.data' DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl SEGMENT REJECT LIMIT 1000;
SELECT * FROM sreh_copy ORDER BY a,b,c;
SELECT relname,linenum,errmsg,rawdata FROM sreh_copy_errtbl ORDER BY linenum,rawdata;

--
-- error table - do the same thing again. this time error table exists and should get reused and data appended
--
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data1.data' DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl SEGMENT REJECT LIMIT 1000;
SELECT * FROM sreh_copy ORDER BY a,b,c;
SELECT relname,linenum,errmsg,rawdata FROM sreh_copy_errtbl ORDER BY linenum,rawdata;

--
-- error table - do the same thing again. this time use data from STDIN (should show in errtable)
--
COPY sreh_copy FROM STDIN DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl SEGMENT REJECT LIMIT 1000;
100|100|100
200 bad data from stdin
300|300|300
400|400| more bad data from stdin
\.
SELECT * FROM sreh_copy ORDER BY a,b,c;
SELECT relname,linenum,errmsg,rawdata FROM sreh_copy_errtbl ORDER BY linenum,rawdata;

--
-- error table - have low reject limit that gets reached, and see that data rolled back from target table and appended to err table
--
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data1.data' DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl SEGMENT REJECT LIMIT 2;
SELECT * FROM sreh_copy ORDER BY a,b,c;
-- the error table will have different count of rows in different array configurations, as there's
-- a chance that arrays with more segdbs recorded more bad rows before one of the segments reached
-- the reject limit. Our only way to make sure data was indeed appended at ABORT is to count the 
-- number of distinct inserts into the error table. this is the 4th one, so must be 4.
SELECT count(distinct(cmdtime)) FROM sreh_copy_errtbl;

--
-- autodrop - error table auto-dropped when auto created and no bad data
--
DROP TABLE sreh_copy_errtbl; 
COPY sreh_copy FROM STDIN DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl SEGMENT REJECT LIMIT 1000;
1|1|1
\.
SELECT * FROM sreh_copy_errtbl; -- it should not exist

--
-- autodrop - error table not auto-dropped when KEEP specified
--
COPY sreh_copy FROM STDIN DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl KEEP SEGMENT REJECT LIMIT 1000;
1|1|1
\.
SELECT * FROM sreh_copy_errtbl; -- should exist and be empty

--
-- autodrop - error table not auto-dropped when it wasn't auto generated, even if no bad data
--
COPY sreh_copy FROM STDIN DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl SEGMENT REJECT LIMIT 1000;
1|1|1
\.
SELECT * FROM sreh_copy_errtbl; -- should exist and be empty

--
-- autodrop - error table not auto-dropped when there is bad data, even if auto-generated
--
DROP TABLE sreh_copy_errtbl; 
COPY sreh_copy FROM STDIN DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl SEGMENT REJECT LIMIT 1000;
1|bad|1
\.
SELECT rawdata FROM sreh_copy_errtbl; -- should exist and have 1 row

--
-- constraint errors - data is rolled back from target table (CHECK)
--
TRUNCATE sreh_copy_errtbl;
CREATE TABLE sreh_constr(a int check (a > 10));
COPY sreh_constr FROM STDIN DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl KEEP SEGMENT REJECT LIMIT 1000;
12
bad format (not int) data should get inserted into error table but later rolledback
11
10
9
\.
SELECT COUNT(*) FROM sreh_copy_errtbl; -- should exist and have 1 bad format row
SELECT * FROM sreh_constr; -- should exist and be empty
DROP TABLE sreh_constr;

--
-- constraint errors - data is rolled back from target table (UNIQUE)
--
TRUNCATE sreh_copy_errtbl;
CREATE TABLE sreh_constr(a int unique);
COPY sreh_constr FROM STDIN DELIMITER '|' LOG ERRORS INTO sreh_copy_errtbl KEEP SEGMENT REJECT LIMIT 1000;
12
bad format (not int) data should get inserted into error table but later rolledback
11
9
9
\.
SELECT COUNT(*) FROM sreh_copy_errtbl; -- should exist and have 1 bad format row
SELECT * FROM sreh_constr; -- should exist and be empty

--
-- test PERCENT reject limit logic with threshold 100.
-- (we don't test the default threshold (300) yet. we could if needed).
-- bad records in bad_data3.data (300 rows): 5,15,55,56,107,136,160,213,214,286
-- this data file is made with errors that will occur only on the QD (with COPY)
-- so the percent calculation should always be the same regardless of number of
-- QE's in the system.
--
set gp_reject_percent_threshold = 100;
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data3.data' DELIMITER '|' SEGMENT REJECT LIMIT 10 PERCENT; --pass
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data3.data' DELIMITER '|' SEGMENT REJECT LIMIT 2 PERCENT; --fail

--
-- test PERCENT reject limit logic with custom threshold 10 (only practical for test purposes)
--
set gp_reject_percent_threshold = 10;
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data3.data' DELIMITER '|' SEGMENT REJECT LIMIT 10 PERCENT; --fail
COPY sreh_copy FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/bad_data3.data' DELIMITER '|' SEGMENT REJECT LIMIT 20 PERCENT; --pass

-- cleanup
DROP TABLE sreh_copy;
DROP TABLE sreh_constr;
DROP TABLE sreh_copy_errtbl;

-- ###########################################################
-- External Tables 
-- ###########################################################

CREATE EXTERNAL WEB TABLE gpfdist_start (x text)
execute E'(($GPHOME/bin/gpfdist -p 8080 -d /home/jsoedomo/perforce/cdbfast/main/partition/_data  </dev/null >/dev/null 2>&1 &); sleep 7; echo "starting...") '
on SEGMENT 0
FORMAT 'text' (delimiter '|');

CREATE EXTERNAL WEB TABLE gpfdist_stop (x text)
execute E'(/bin/pkill gpfdist || killall gpfdist) > /dev/null 2>&1; echo "stopping..."'
on SEGMENT 0
FORMAT 'text' (delimiter '|');

select * from gpfdist_stop;
select * from gpfdist_start;

-- create a target table to load into
CREATE TABLE sreh_target(a int, b int, c int) distributed by(a);

-- 
-- reject limit only
--
CREATE EXTERNAL TABLE sreh_ext(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data1.data' )
FORMAT 'text' (delimiter '|')
SEGMENT REJECT LIMIT 10000;

SELECT * FROM sreh_ext;
INSERT INTO sreh_target SELECT * FROM sreh_ext;
SELECT count(*) FROM sreh_target;
TRUNCATE sreh_target;
DROP EXTERNAL TABLE sreh_ext;

-- 
-- reject limit only - low value that gets reached
--
CREATE EXTERNAL TABLE sreh_ext(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data1.data' )
FORMAT 'text' (delimiter '|')
SEGMENT REJECT LIMIT 2;

SELECT * FROM sreh_ext ORDER BY a;
INSERT INTO sreh_target SELECT * FROM sreh_ext;
SELECT count(*) FROM sreh_target;
TRUNCATE sreh_target;
DROP EXTERNAL TABLE sreh_ext;

-- 
-- error table (also testing that it gets automatically generated)
--
CREATE EXTERNAL TABLE sreh_ext(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data1.data' )
FORMAT 'text' (delimiter '|')
LOG ERRORS INTO sreh_ext_errtbl
SEGMENT REJECT LIMIT 1000;

SELECT * FROM sreh_ext ORDER BY a;
SELECT relname,errmsg,rawdata FROM sreh_ext_errtbl ORDER BY errmsg,rawdata;
TRUNCATE sreh_ext_errtbl;

INSERT INTO sreh_target SELECT * FROM sreh_ext;
SELECT count(*) FROM sreh_target;
SELECT relname,errmsg,rawdata FROM sreh_ext_errtbl ORDER BY errmsg,rawdata;
TRUNCATE sreh_ext_errtbl;
TRUNCATE sreh_target;
DROP EXTERNAL TABLE sreh_ext;

--
-- error table - do the same thing again. this time error table exists and should get reused
--
CREATE EXTERNAL TABLE sreh_ext(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data1.data' )
FORMAT 'text' (delimiter '|')
LOG ERRORS INTO sreh_ext_errtbl
SEGMENT REJECT LIMIT 1000;

SELECT * FROM sreh_ext ORDER BY a;
SELECT relname,errmsg,rawdata FROM sreh_ext_errtbl ORDER BY errmsg,rawdata;
TRUNCATE sreh_ext_errtbl;

INSERT INTO sreh_target SELECT * FROM sreh_ext;
SELECT count(*) FROM sreh_target;
SELECT relname,errmsg,rawdata FROM sreh_ext_errtbl ORDER BY errmsg,rawdata;
TRUNCATE sreh_target;
TRUNCATE sreh_ext_errtbl;
DROP EXTERNAL TABLE sreh_ext;

--
-- constraint errors - data is rolled back from both target and error tables (CHECK)
--
CREATE TABLE sreh_constr(a int, b int, c int check (c < 10));
CREATE EXTERNAL TABLE sreh_ext(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data1.data' )
FORMAT 'text' (delimiter '|')
LOG ERRORS INTO sreh_ext_errtbl
SEGMENT REJECT LIMIT 1000;

INSERT INTO sreh_constr SELECT * FROM sreh_ext;
SELECT COUNT(*) FROM sreh_ext_errtbl; -- should be empty
SELECT COUNT(*) FROM sreh_constr; -- should be empty

--
-- test PERCENT reject limit logic with threshold 100.
-- (we don't test the default threshold (300) yet. we could if needed).
-- bad records in bad_data3.data (300 rows): 5,15,55,56,107,136,160,213,214,286
-- in here we assume that gpfdist will distribute all the data to one QE (since
-- it is much smaller than the data buffer it uses) so the percent calculation 
-- should always be the same regardless of number of QE's in the system.
--
set gp_reject_percent_threshold = 100;
CREATE EXTERNAL TABLE sreh_ext_10percent(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data3.data' )
FORMAT 'text' (delimiter '|')
SEGMENT REJECT LIMIT 10 PERCENT;

SELECT count(*) FROM sreh_ext_10percent; -- pass

CREATE EXTERNAL TABLE sreh_ext_2percent(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data3.data' )
FORMAT 'text' (delimiter '|')
SEGMENT REJECT LIMIT 2 PERCENT;

SELECT count(*) FROM sreh_ext_2percent; -- fail

--
-- test PERCENT reject limit logic with custom threshold 10 (only practical for test purposes)
--
set gp_reject_percent_threshold = 10;
SELECT count(*) FROM sreh_ext_10percent; -- fail

CREATE EXTERNAL TABLE sreh_ext_20percent(a int, b int, c int)
LOCATION ('gpfdist://localhost:8080/bad_data3.data' )
FORMAT 'text' (delimiter '|')
SEGMENT REJECT LIMIT 20 PERCENT;

SELECT count(*) FROM sreh_ext_20percent; -- pass
set gp_reject_percent_threshold = 300; -- reset

-- cleanup
select * from gpfdist_stop;
DROP EXTERNAL WEB TABLE gpfdist_stop;
DROP EXTERNAL WEB TABLE gpfdist_start;
DROP EXTERNAL TABLE sreh_ext_2percent;
DROP EXTERNAL TABLE sreh_ext_10percent;
DROP EXTERNAL TABLE sreh_ext_20percent;
DROP TABLE sreh_target;
DROP TABLE sreh_ext_errtbl; -- should fail by dependency
DROP TABLE sreh_ext_errtbl CASCADE;
