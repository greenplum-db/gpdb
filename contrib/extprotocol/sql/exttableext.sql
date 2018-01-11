SET search_path TO 'exttableext';
-- Test 3: create RET and WET using created protocol

    -- Create external RET and WET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_options_r;
    CREATE READABLE EXTERNAL TABLE exttabtest_options_r(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text'
    OPTIONS (database 'redplum');

    SELECT * FROM exttabtest_options_r
    EXCEPT ALL
    SELECT * FROM exttabtest;

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r;
    CREATE READABLE EXTERNAL TABLE exttabtest_r(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text'
    OPTIONS (database 'greenplum', foo 'bar');

    -- Checking pg_exttable for new created RET and WET
    select urilocation,fmttype,fmtopts,encoding,writable from pg_exttable 
    where reloid='exttabtest_r'::regclass 
       or reloid='exttabtest_w'::regclass;

    -- write to WET
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w (SELECT * FROM exttabtest);

    -- read from RET
    SELECT * FROM exttabtest_r
    EXCEPT ALL
    SELECT * FROM exttabtest;

-- verify data should be evenly distributed
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from exttabtest_r group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

-- Test 4.1: create uni-directional write protocol
-- create WET using created protocol

    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        writefunc = write_to_file_stable
    );

    -- Create WET with uni-directional protocol
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_uni;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_uni(like exttabtest)
        LOCATION('demoprot://exttabtest_uni.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- write to WET
    INSERT INTO exttabtest_w_uni (SELECT * FROM exttabtest);

-- Test 4.2: create uni-directional read protocol 
-- create RET using created protocol

    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable
    );

    -- Create RET with uni-directional protocol
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_uni;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_uni(like exttabtest)
        LOCATION('demoprot://exttabtest_uni.txt') 
    FORMAT 'text';

    -- read from RET
    SELECT * FROM exttabtest_r_uni
    EXCEPT ALL
    SELECT * FROM exttabtest;
-- Test 5: using bi-directional protocol, create ext table with different
-- distribution policy than the source table
-- When exporting, date file (exttabtest_dist.txt) will only be created as necessary. 
-- When data is not evenly distributed (as for this test case), 
-- some segments will not have data file created.
--
-- When importing, data file is required for each primary segment. 
-- Otherwise "ERROR:  demoprot_import: could not open file " will be thrown.

    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_dist;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_dist(like exttabtest)
        LOCATION('demoprot://exttabtest_dist.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (value2);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_dist;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_dist(like exttabtest)
        LOCATION('demoprot://exttabtest_dist.txt') 
    FORMAT 'text';

    -- write to WET
    INSERT INTO exttabtest_w_dist (SELECT * FROM exttabtest);

    -- read from RET
    SELECT * FROM exttabtest_r_dist
    EXCEPT ALL
    SELECT * FROM exttabtest;

-- verify data should be evenly distributed
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from exttabtest_r_dist group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

-- Test 6: using two urls and using CSV format

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_2url;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_2url (like exttabtest)
        LOCATION('demoprot://exttabtest_2url_1.csv', 
                 'demoprot://exttabtest_2url_2.csv') 
    FORMAT 'csv';

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_2url;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_2url (like exttabtest)
        LOCATION('demoprot://exttabtest_2url_1.csv', 
                 'demoprot://exttabtest_2url_2.csv')  
    FORMAT 'csv'
    DISTRIBUTED BY (id);

    -- write to WET
    INSERT INTO exttabtest_w_2url (SELECT * FROM exttabtest);

    -- read from RET
    SELECT * FROM exttabtest_r_2url
    EXCEPT ALL
    SELECT * FROM exttabtest;

-- verify data should be evenly distributed
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from exttabtest_r_2url group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

    -- Check the output file at each segments
    -- ! gpssh -f allsegs ls -l /data/hhuang/MAIN/main_debug/primary/gpseg*/exttabtest_2url*.csv
-- Test 7: using two urls and text format
    
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_2url;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_2url (like exttabtest)
        LOCATION('demoprot://exttabtest_2url_1.txt', 
                 'demoprot://exttabtest_2url_2.txt')
    FORMAT 'text';

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_2url;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_2url (like exttabtest)
        LOCATION('demoprot://exttabtest_2url_1.txt', 
                 'demoprot://exttabtest_2url_2.txt')
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- write to WET
    INSERT INTO exttabtest_w_2url (SELECT * FROM exttabtest);

    -- read from RET
    SELECT * FROM exttabtest_r_2url
    EXCEPT ALL
    SELECT * FROM exttabtest;

-- verify data should be evenly distributed
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from exttabtest_r_2url group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

    -- Checking the output files on segments
    -- ! gpssh -f allsegs ls -l /data/hhuang/MAIN/main_debug/primary/gpseg*/exttabtest_2url*.txt
-- Test 8: Negative - using 5 urls, exceeding number of primary segments (4)

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_5url;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_5url (like exttabtest)
        LOCATION('demoprot://exttabtest_5url_1.txt', 
                 'demoprot://exttabtest_5url_2.txt',
                 'demoprot://exttabtest_5url_3.txt',
                 'demoprot://exttabtest_5url_4.txt',
                 'demoprot://exttabtest_5url_5.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- write to WET
    INSERT INTO exttabtest_w_5url (SELECT * FROM exttabtest);
-- @skip Skipping this test because of a duplicate key violation error.
-- Test 9: Negative - duplicte protocol name (check pg_extprotocol)

    SELECT count(*) FROM pg_extprotocol WHERE ptcname='demoprot';

    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );
-- Test 10: Negative: create external table using non-existing protocol

    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_neg10 (like exttabtest)
        LOCATION('demoprot_nonexist://exttabtest_neg10.txt')  
    FORMAT 'text'
    DISTRIBUTED BY (id);
-- Test 11: Negative - Using invalid protocol attribute name
-- attribute names must be readproc, write proc, and validatorproc

    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunction  = read_from_file, 
        writefunction = write_to_file
    );
-- Test 12: Negatvie - using undefined function when defining protocol

    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc  = read_from_file_badname, 
        writefunc = write_to_file_badname
    );
-- Test 13: Negatvie - syntax error: missing '=' when defining protocol

    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc read_from_file, 
        writefunc write_to_file
    );
-- @skip Test disabled in cdbfast as well
-- Test 14: Negative - switching read function and write function
-- This is user error. GPDB should display meaningful error message.
-- Not running this test. Comment from cdbfast - Comment this out since it doesn't make much sense and it creates big output files
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        writefunc = read_from_file_stable, 
        readfunc = write_to_file_stable
    );

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_switched;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_switched (like exttabtest)
        LOCATION('demoprot://exttabtest_switched.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_switched;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_switched(like exttabtest)
        LOCATION('demoprot://exttabtest_switched.txt') 
    FORMAT 'text';

    -- write to WET
    INSERT INTO exttabtest_w_switched (SELECT * FROM exttabtest);

    -- read from RET
    --SELECT * FROM exttabtest_r_switched
    --EXCEPT ALL
    --SELECT * FROM exttabtest;
-- Test 15: Negative - circular reference
-- write to WET while selecting from RET, and WET and RET are using the same data source files
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_circle;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_circle(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_circle;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_circle(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text';

    -- First to create exttabtest.txt using WET
    INSERT INTO exttabtest_w_circle (SELECT * FROM exttabtest);

    -- write to WET while reading from RET, 
    -- using limit to avoid infinit loop 
    -- This is to test that using RET and WET inappropriately can get yourself into trouble
    INSERT INTO exttabtest_w_circle (SELECT * FROM exttabtest_r_circle order by id limit 100);

-- Test 16: Negative - invalid URL: missing path
    drop external table if exists exttabtest_w_misspath;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_misspath(like exttabtest)
        LOCATION('demoprot://') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    insert into exttabtest_w_misspath (select * from exttabletest);
-- Test 17: Negative - invalid URL: missing protocol

    CREATE READABLE EXTERNAL TABLE exttabtest_r_missprot(like exttabtest)
        LOCATION('exttabtest.txt') 
    FORMAT 'text';

-- Test 18: Negative - invalid URL: invalid path

    CREATE READABLE EXTERNAL TABLE exttabtest_r_invalidpath(like exttabtest)
        LOCATION('demoprot:\\exttabtest.txt') 
    FORMAT 'text';

-- Test 19: Negative - invalid URL: invalid protocol name

    CREATE READABLE EXTERNAL TABLE exttabtest_r_invalidprot(like exttabtest)
        LOCATION('badprotocol://exttabtest.txt') 
    FORMAT 'text';

-- Test 20: Small dataset - 20 records

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_20records;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_20records (like exttabtest)
        LOCATION('demoprot://exttabtest_20records.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_20records;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_20records (like exttabtest)
        LOCATION('demoprot://exttabtest_20records.txt') 
    FORMAT 'text';

    -- write to WET
    INSERT INTO exttabtest_w_20records (SELECT * FROM exttabtest where id<=20);

    -- read from RET
    SELECT * FROM exttabtest_r_20records order by id;

-- verify data should be evenly distributed
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from exttabtest_r_20records group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

    -- Drop External Tables
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_20records;
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_20records;
-- Test 21: Small dataset - 1 record

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_1record;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_1record (like exttabtest)
        LOCATION('demoprot://exttabtest_1record.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_1record;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_1record (like exttabtest)
        LOCATION('demoprot://exttabtest_1record.txt') 
    FORMAT 'text';

    -- write to WET
    INSERT INTO exttabtest_w_1record (SELECT * FROM exttabtest where id = 4);

    -- read from RET
    -- The implemented example protocol (demoprot) requires data file must be available
    -- (even it is empty) for each (primary) segment.
    -- This is the limitation of this example (MPP-13811)
    -- and will cause following SElECT query to fail
    SELECT * FROM exttabtest_r_1record order by id;

    -- Drop External Tables
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_1record;
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_1record;

-- Test 22: Using /dev/null

    -- Using /dev/null for output and input
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_null;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_null (like exttabtest)
        LOCATION('demoprot:///dev/null') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_null;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_null (like exttabtest)
        LOCATION('demoprot:///dev/null') 
    FORMAT 'text';

    -- write to WET
    INSERT INTO exttabtest_w_null (SELECT * FROM exttabtest);

    -- read from RET
    SELECT count(id) FROM exttabtest_r_null;


-- Test 23: Performance - 1M records

    -- Load 1M rows of data
    TRUNCATE TABLE exttabtest;
    INSERT INTO exttabtest SELECT i, 'name'||i, i*2, i*3 FROM generate_series(1,1000000) i;

    -- Using demoprot, import and export 1M records        
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_1M;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_1M (like exttabtest)
        LOCATION('demoprot://exttabtest_1M.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_1M;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_1M (like exttabtest)
        LOCATION('demoprot://exttabtest_1M.txt') 
    FORMAT 'text';

    -- write to WET
    INSERT INTO exttabtest_w_1M (SELECT * FROM exttabtest);
    -- Time: 1369.028 ms

    -- read from RET
    SELECT count(id) FROM exttabtest_r_1M;
    -- Time: 869.793 ms

    -- Compare to using demoprot protocol and output to /dev/null (no disk IO, thanks to Alan)
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_1M_null;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_1M_null (like exttabtest)
        LOCATION('demoprot:///dev/null') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_1M_null;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_1M_null (like exttabtest)
        LOCATION('demoprot:///dev/null') 
    FORMAT 'text';

    -- write to WET
    INSERT INTO exttabtest_w_1M_null (SELECT * FROM exttabtest);
    -- Time: 1368.173 ms (about same as writing to file - CPU intensive)
-- Test 30: UDF dependency - drop UDF when it does not have dependent protocol

    -- Create new UDF that has no dependent
    CREATE OR REPLACE FUNCTION write_udf_todrop() RETURNS integer AS
        '$libdir/gpextprotocol.so', 'demoprot_export' LANGUAGE C STABLE;

    CREATE OR REPLACE FUNCTION read_udf_todrop() RETURNS integer AS 
        '$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE;

    -- Check pg_proc catalog table for new created functions
    SELECT proname, prolang,proisstrict,provolatile,pronargs,prorettype,prosrc,proacl FROM pg_proc
    WHERE proname like 'write_udf_todrop'
       or proname like 'read_udf_todrop'
    ORDER BY proname;

    -- Drop two UDFs
    DROP FUNCTION write_udf_todrop();
    DROP FUNCTION read_udf_todrop();

    -- Check pg_proc catalog table for after drop the UDFs
    SELECT proname, prolang,proisstrict,provolatile,pronargs,prorettype,prosrc,proacl FROM pg_proc
    WHERE proname like 'write_udf_todrop'
       or proname like 'read_udf_todrop'
    ORDER BY proname;
-- Test 31: UDF dependency - drop UDF when it has dependent dependent protocol
-- Cannot drop UDF when it has protocol dependent.

    -- Check protocol demoprot is depending on UDF
    select count(*) from pg_extprotocol
    where ptcreadfn='read_from_file_stable'::regproc;

    -- Try to drop UDF read_from_file_stable() that has protocol demoprot depends on it
    drop function read_from_file_stable();

    -- Check pg_proc catalog table to verify UDF is NOT dropped
    SELECT proname, prolang,proisstrict,provolatile,pronargs,prorettype,prosrc,proacl FROM pg_proc
    WHERE proname like 'write_to_file%'
       or proname like 'read_from_file%'
    ORDER BY proname;
-- Test 35: UDF dependency - drop function cascade

    -- Restore (recreate) protocol with the same protocol name demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );
    -- CREATE PROTOCOL

    -- Drop function cascade.
    -- Showing notice message of cascade drop of dependent protocol demoprot
    drop function read_from_file_stable() cascade;
    -- NOTICE:  drop cascades to protocol demoprot
    -- DROP FUNCTION

    -- Check pg_proc for function read_from_file_stable
    SELECT oid, proname FROM pg_proc 
    WHERE proname = 'read_from_file_stable';
    -- returns 0

    -- Check dependency: pg_depend table
    select count(*) from pg_depend 
    where objid in (
        select oid from pg_extprotocol where ptcname='demoprot');

    -- Verified other catalog tables: pg_extprotocol, pg_depend
    -- Check pg_extprotocol table, the entry should be dropped
    select count(*) from pg_extprotocol where ptcname='demoprot';
    -- returns 0

-- Test 36: Regression - Alter Ext Table when using custom protocol

    -- Recreate function read_from_file_stable
    CREATE OR REPLACE FUNCTION read_from_file_stable() RETURNS integer AS 
        '$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE;

    -- Restore (recreate) protocol with the same protocol name demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );
    -- CREATE PROTOCOL

    CREATE READABLE EXTERNAL TABLE exttabtest_r(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text';

    CREATE WRITABLE EXTERNAL TABLE exttabtest_w(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- truncate table exttabtest and load 100 records
    TRUNCATE TABLE exttabtest;
    INSERT INTO exttabtest SELECT i, 'name'||i, i*2, i*3 FROM generate_series(1,100) i;

    -- Check existing WET that using protocol demoprot
    SELECT * FROM clean_exttabtest_files;
    insert into exttabtest_w (select * from exttabtest);

    -- Check existing RET that using protocol demoprot
    select * from exttabtest_r where id <=4 order by id;
    -- returns 4 records

    -- Change ext table name when using custom protocol
    ALTER TABLE exttabtest_r rename to exttabtest_r_newname;

    -- Check for Add|Drop|Rename column
    ALTER EXTERNAL TABLE exttabtest_r_newname ADD COLUMN value3 int;
    ALTER TABLE exttabtest_r_newname RENAME COLUMN value3 to value3_newname;
    ALTER EXTERNAL TABLE exttabtest_r_newname DROP COLUMN value3_newname;

    -- Check external table is still accessible after alter operation
    select * from exttabtest_r_newname where id <=4 order by id;
    -- returns 4 records

-- Test 37: Regression - Drop Ext Table when using custom protocol

    -- Check external table exttabtest_r_newname exists
    -- and is using demoprot protocol
    \d exttabtest_r_newname
    select count(*) from pg_class where relname = 'exttabtest_r_newname';

    -- Drop external table 'exttabtest_r_newname'
    DROP EXTERNAL TABLE exttabtest_r_newname;
    -- DROP EXTERNAL TABLE

-- Test 38: Negative - handling invalid data formate when using custom protocol

    -- Recreate RET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_invalid;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_invalid(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text';

    -- Using WET to create data source file exttabtest.txt
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w (select * from exttabtest);

    -- Check RET is accessible
    SELECT count(*) from exttabtest_r_invalid;
    -- returns count = 100

    -- Add a column (value3) to RET
    ALTER EXTERNAL TABLE exttabtest_r_invalid ADD COLUMN value3 int;

    -- Access RET again with changed structure, the data file format is invalid now
    -- Comment this out since output file is undeterministic
    -- SELECT count(*) from exttabtest_r_invalid;

    -- Drop a column (id) from RET
    ALTER EXTERNAL TABLE exttabtest_r_invalid DROP COLUMN id;

    -- Access RET again with changed structure, the data file format is also invalid
    -- Comment this out since output file is undeterministic
    -- SELECT count(*) from exttabtest_r_invalid where gp_segment_id=0;

    -- Recreate WET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_invalid;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_invalid(like exttabtest)
        LOCATION('demoprot://exttabtest_invalid.txt') 
    FORMAT 'text';

    -- Drop a column (id) from WET
    ALTER EXTERNAL TABLE exttabtest_w_invalid DROP COLUMN id;

    -- Write to WET 
    INSERT INTO exttabtest_w_invalid (SELECT * from exttabtest);

-- Test 60: setup source table
-- This table is our example database table for formatter test

DROP TABLE IF EXISTS formatsource CASCADE;
CREATE TABLE formatsource(
   name       varchar(40),
   id       float8,
   value1   float8,
   value2   float8
) 
DISTRIBUTED BY (id);

-- Loading 100 records

    \echo 'loading data...'
    INSERT INTO formatsource SELECT 'name'||i,  i, i*2, i*3 FROM generate_series(1,100) i;

-- Check data distribution
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from formatsource group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

-- Test 61: create STABLE read and write functions based on example gpformatter.so
-- Note: Only STABLE is supported for formatter.
-- When it is not STABLE (VALOTILE, or IMMUTABLE),
-- the expected output of CREATE EXTERNAL TABLE should be an error.
CREATE OR REPLACE FUNCTION formatter_export_s(record) RETURNS bytea 
    AS '$libdir/gpformatter.so', 'formatter_export'
LANGUAGE C STABLE;

CREATE OR REPLACE FUNCTION formatter_import_s() RETURNS record
    AS '$libdir/gpformatter.so', 'formatter_import'
LANGUAGE C STABLE;

CREATE OR REPLACE FUNCTION formatter_export_v(record) RETURNS bytea 
    AS '$libdir/gpformatter.so', 'formatter_export'
LANGUAGE C VOLATILE;

CREATE OR REPLACE FUNCTION formatter_import_v() RETURNS record
    AS '$libdir/gpformatter.so', 'formatter_import'
LANGUAGE C VOLATILE;

CREATE OR REPLACE FUNCTION formatter_export_i(record) RETURNS bytea 
    AS '$libdir/gpformatter.so', 'formatter_export'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION formatter_import_i() RETURNS record
    AS '$libdir/gpformatter.so', 'formatter_import'
LANGUAGE C IMMUTABLE;

-- Check pg_proc catalog table for new created functions
    SELECT proname, prolang,proisstrict,provolatile,pronargs,prorettype,prosrc,proacl FROM pg_proc 
    WHERE proname like 'formatter%' 
    ORDER BY proname;
-- Test 62: Drop function without dependent external table

CREATE OR REPLACE FUNCTION formatter_export_todrop(record) RETURNS bytea 
    AS '$libdir/gpformatter.so', 'formatter_export'
LANGUAGE C STABLE;

CREATE OR REPLACE FUNCTION formatter_import_todrop() RETURNS record
    AS '$libdir/gpformatter.so', 'formatter_import'
LANGUAGE C STABLE;

DROP FUNCTION formatter_export_todrop(record);

DROP FUNCTION formatter_import_todrop();

-- Test 63: create RET and WET using demoprot protocol and STABLE formatter
-- Note: Only STABLE is supported for formatter, this is enforced
-- When it is not STABLE (VOLATILE, or IMMUTABLE),
-- the expected error like:
-- ERROR: formatter function formatter_export_i is not declared STABLE. (seg1 rh55-qavm55:7533 pid=14816)

    -- Create RET and WET using IMMUTABLE functions will succeed
    -- However query such RET or WET should fail
    DROP EXTERNAL TABLE IF EXISTS format_w;
    CREATE WRITABLE EXTERNAL TABLE format_w(like formatsource) 
    LOCATION ('demoprot://exttabtest_test63') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_i');

    DROP EXTERNAL TABLE IF EXISTS format_r;
    CREATE READABLE EXTERNAL TABLE format_r(like formatsource) 
    LOCATION ('demoprot://exttabtest_test63') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_i');

    INSERT INTO format_w (SELECT * FROM formatsource);
    -- ERROR:  formatter function formatter_export_i is not declared STABLE.  (seg1 rh55-qavm55:7533 pid=14816)

    -- Create RET and WET using STABLE functions 
    DROP EXTERNAL TABLE IF EXISTS format_w;
    CREATE WRITABLE EXTERNAL TABLE format_w(like formatsource) 
    LOCATION ('demoprot://exttabtest_test63') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_r;
    CREATE READABLE EXTERNAL TABLE format_r(like formatsource) 
    LOCATION ('demoprot://exttabtest_test63') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Displaying table info
    \d format_w
    \d format_r

    -- Checking pg_exttable 
    select pg_class.relname, fmttype,
       fmtopts, encoding, writable
    from pg_class, pg_exttable
    where pg_class.oid = pg_exttable.reloid
    and (pg_class.relname='format_w'
    or
    pg_class.relname='format_r'
    or
    pg_class.relname='format_text'
    or
    pg_class.relname='format_csv');

    -- Write to WET
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO format_w (SELECT * FROM formatsource);

    -- read from RET
    SELECT * FROM format_r
    EXCEPT ALL
    SELECT * FROM formatsource;

    -- Read from RET 
    SELECT count(*) FROM format_r;

-- verify data should be evenly distributed
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from format_r group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

-- Test 64: Drop format function with external table using the function

    DROP FUNCTION formatter_export_i(record); 
    DROP FUNCTION formatter_import_i();
    DROP FUNCTION formatter_export_s(record); 
    DROP FUNCTION formatter_import_s();

    -- Check pg_proc catalog table for the format functions
    -- should return 0
    SELECT proname, prolang,proisstrict,provolatile,
           pronargs,prorettype,prosrc,proacl FROM pg_proc 
    WHERE proname='formatter_import_s' or proname='formatter_export_s' 
    ORDER BY proname;

    -- Checking pg_exttable, should return 0
    select pg_class.relname, fmttype,
       fmtopts, encoding, writable
    from pg_class, pg_exttable
    where pg_class.oid = pg_exttable.reloid
    and (pg_class.relname='format_w'
    or
    pg_class.relname='format_r');

    -- Write to WET, should fail
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO format_w (SELECT * FROM formatsource);

    -- Read from RET, should fail
    SELECT count(*) FROM format_r;
-- Test 65: Restore (recreate) functions with same name, external table should work again

    -- Recreate UDFs with same function names
    CREATE OR REPLACE FUNCTION formatter_export_s(record) RETURNS bytea 
        AS '$libdir/gpformatter.so', 'formatter_export'
    LANGUAGE C STABLE;

    CREATE OR REPLACE FUNCTION formatter_import_s() RETURNS record
        AS '$libdir/gpformatter.so', 'formatter_import'
    LANGUAGE C STABLE;

    -- Check pg_proc catalog table for new created functions
    SELECT proname, prolang,proisstrict,provolatile,
           pronargs,prorettype,prosrc,proacl FROM pg_proc 
    WHERE proname='formatter_import_s' or proname='formatter_export_s' 
    ORDER BY proname;

    -- Write to WET
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO format_w (SELECT * FROM formatsource);

    -- Read from RET
    SELECT count(*) FROM format_r;
    -- returns count = 100
-- Test 67: Multiple external tables can use same UDF independently

    -- Recreate STABLE import and export UDFs
    CREATE OR REPLACE FUNCTION formatter_export_s(record) RETURNS bytea 
        AS '$libdir/gpformatter.so', 'formatter_export'
    LANGUAGE C STABLE;

    CREATE OR REPLACE FUNCTION formatter_import_s() RETURNS record
        AS '$libdir/gpformatter.so', 'formatter_import'
    LANGUAGE C STABLE;

    -- First pair of RET and WET that using 
    -- formatter_import_s and formatter_export_s
    DROP EXTERNAL TABLE IF EXISTS format_w_s1;
    CREATE WRITABLE EXTERNAL TABLE format_w_s1(like formatsource) 
        LOCATION ('demoprot://exttabtest_test67_s1') 
        FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_r_s1;
    CREATE READABLE EXTERNAL TABLE format_r_s1(like formatsource) 
    LOCATION ('demoprot://exttabtest_test67_s1') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Second pair of RET and WET that using 
    -- same formatter_import_s and formatter_export_s
    DROP EXTERNAL TABLE IF EXISTS format_w_s2;
    CREATE WRITABLE EXTERNAL TABLE format_w_s2(like formatsource) 
        LOCATION ('demoprot://exttabtest_test67_s2') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_r_s2;
    CREATE READABLE EXTERNAL TABLE format_r_s2(like formatsource) 
    LOCATION ('demoprot://exttabtest_test67_s2') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Checking pg_exttable
    select pg_class.relname, fmttype,
       fmtopts, encoding, writable
    from pg_class, pg_exttable
    where pg_class.oid = pg_exttable.reloid
    and (pg_class.relname like 'format_w_s%'
        or
        pg_class.relname like 'format_r_s%');

    -- Write to WET    
    INSERT INTO format_w_s1 (SELECT * FROM formatsource);
    INSERT INTO format_w_s2 (SELECT * FROM formatsource);

    -- read from RET
    SELECT * FROM format_r_s1
    EXCEPT ALL
    SELECT * FROM formatsource;

    SELECT * FROM format_r_s2
    EXCEPT ALL
    SELECT * FROM formatsource;

    -- Read from RET
    SELECT count(*) FROM format_r_s1;
    SELECT count(*) FROM format_r_s2;

-- Test 71: Check limit of MAX_FORMAT_STRING 4096 bytes for variable length strings - text type

    --  Create format_long table for formatter long record test
    DROP TABLE IF EXISTS format_long CASCADE;
    CREATE TABLE format_long (
       id       float8,
       name     text
    ) DISTRIBUTED by (id);

    -- Check the atttypmod of column name is -1
    SELECT atttypmod FROM pg_attribute
    WHERE attrelid = 'format_long'::regclass
    AND attname = 'name';
    -- returns atttypmod = -1

    -- Create RET and WET using demoprot and custom format UDFs
    DROP EXTERNAL TABLE IF EXISTS format_long_w;
    CREATE WRITABLE EXTERNAL TABLE format_long_w(like format_long) 
    LOCATION ('demoprot://exttabtest_test71.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_long_r;
    CREATE READABLE EXTERNAL TABLE format_long_r(like format_long) 
    LOCATION ('demoprot://exttabtest_test71.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Loading each record with 4000 characters, less than 4096 bytes
    truncate table format_long;
    insert into format_long select 1.0,i from repeat('oxo1', 1000) i;
    insert into format_long select 2.0,i from repeat('oxo2', 1000) i;
    insert into format_long select 3.0,i from repeat('oxo3', 1000) i;
    insert into format_long select 4.0,i from repeat('oxo4', 1000) i;
    insert into format_long select 5.0,i from repeat('oxo5', 1000) i;
    insert into format_long select 6.0,i from repeat('oxo6', 1000) i;
    insert into format_long select 7.0,i from repeat('oxo7', 1000) i;
    insert into format_long select 8.0,i from repeat('oxo8', 1000) i;
    insert into format_long select 9.0,i from repeat('oxo9', 1000) i;

    -- Check distribution is even
with t as (
  SELECT gp_segment_id as segid, count(*) as cnt from format_long group by gp_segment_id
)
select max(cnt) - min(cnt)  > 20 from t;

    -- Write to WET
    -- insert should be successful
    INSERT INTO format_long_w (SELECT * FROM format_long);

    -- Read from RET
    select count(*) from format_long_r;
    -- returns count = 4

    -- Now loading each record with 5000 characters, more than 4096 bytes for each record
    truncate table format_long;
    insert into format_long select 1.0,i from repeat('oxox1', 1000) i;
    insert into format_long select 2.0,i from repeat('oxox2', 1000) i;
    insert into format_long select 3.0,i from repeat('oxox3', 1000) i;
    insert into format_long select 4.0,i from repeat('oxox4', 1000) i;

    -- Write to WET
    -- insert should fail since MAX_FORMAT_STRING (4096) is exceeded
    INSERT INTO format_long_w (SELECT * FROM format_long);
    -- ERROR:  formatter_export: buffer too small (gpformatter.c:182)  (seg2 rh55-qavm58:5532 pid=20093) 

-- Test 72: Verify limit of MAX_FORMAT_STRING 4096 bytes does not apply to fixed length strings - varchar()
-- MAX_FORMAT_STRING limit should not apply to fixed length strings (char or varchar)

    --  Create format_long table for formatter long record test
    DROP TABLE IF EXISTS format_long CASCADE;
    CREATE TABLE format_long (
       id       float8,
       name     varchar(6000)
    ) DISTRIBUTED by (id);

    -- Check the atttypmod of name > 0
    SELECT atttypmod FROM pg_attribute
    WHERE attrelid = 'format_long'::regclass
    AND attname = 'name';
    -- returns atttypmod = 6004

    -- Create RET and WET using demoprot and custom format UDFs
    DROP EXTERNAL TABLE IF EXISTS format_long_w;
    CREATE WRITABLE EXTERNAL TABLE format_long_w(like format_long) 
    LOCATION ('demoprot://exttabtest_test72.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_long_r;
    CREATE READABLE EXTERNAL TABLE format_long_r(like format_long) 
    LOCATION ('demoprot://exttabtest_test72.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Now loading each record with 5000 characters, more than 4096 bytes for each record
    truncate table format_long;
    insert into format_long select 1.0,i from repeat('oxox1', 1000) i;
    insert into format_long select 2.0,i from repeat('oxox2', 1000) i;
    insert into format_long select 3.0,i from repeat('oxox3', 1000) i;
    insert into format_long select 4.0,i from repeat('oxox4', 1000) i;
    insert into format_long select 5.0,i from repeat('oxox5', 1000) i;
    insert into format_long select 6.0,i from repeat('oxox6', 1000) i;
    insert into format_long select 7.0,i from repeat('oxox7', 1000) i;
    insert into format_long select 8.0,i from repeat('oxox8', 1000) i;
    insert into format_long select 9.0,i from repeat('oxox9', 1000) i;

    -- Write to WET
    -- insert should be successful
    INSERT INTO format_long_w (SELECT * FROM format_long);

    -- Read from RET
    select count(*) from format_long_r;
    -- returns count = 9
-- Test 73: Interlacing short and long record, testing FMT_NEED_MORE_DATA
-- When loading data from RET, long record may trigger FORMATTER_RETURN_NOTIFICATION(fcinfo, FMT_NEED_MORE_DATA).
-- Verify the data can be loaded successfully and should exactly match the source records.

    --  Create format_long table for formatter long record test
    DROP TABLE IF EXISTS format_long CASCADE;
    CREATE TABLE format_long (
       id       float8,
       name     varchar(5000)
    ) DISTRIBUTED by (id);

    -- Check the atttypmod of name > 0
    SELECT atttypmod FROM pg_attribute
    WHERE attrelid = 'format_long'::regclass
    AND attname = 'name';
    -- returns atttypmod = 5004

    -- Create RET and WET using demoprot and custom format UDFs
    DROP EXTERNAL TABLE IF EXISTS format_long_w;
    CREATE WRITABLE EXTERNAL TABLE format_long_w(like format_long) 
    LOCATION ('demoprot://exttabtest_test73.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_long_r;
    CREATE READABLE EXTERNAL TABLE format_long_r(like format_long) 
    LOCATION ('demoprot://exttabtest_test73.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Loading short and long records alternatively.
    -- This should trigger FORMATTER_RETURN_NOTIFICATION(fcinfo, FMT_NEED_MORE_DATA)
    -- However not really sure about this.
    truncate table format_long;
    insert into format_long select 1.0,'oxo1';
    insert into format_long select 2.0,i from repeat('oxo2', 1000) i;
    insert into format_long select 3.0,'oxo3';
    insert into format_long select 4.0,i from repeat('oxo4', 1000) i;
    insert into format_long select 5.0,'oxo5';
    insert into format_long select 6.0,i from repeat('oxo6', 1000) i;
    insert into format_long select 7.0,'oxo7';
    insert into format_long select 8.0,i from repeat('oxo8', 1000) i;
    insert into format_long select 9.0,'oxo9';

    -- Write to WET
    -- insert should be successful
    INSERT INTO format_long_w (SELECT * FROM format_long);

    -- Read from RET
    select count(*) from format_long_r;
    -- returns count = 9

    -- read from RET, both should return 0
    SELECT * FROM format_long_r
    EXCEPT ALL
    SELECT * FROM format_long;

-- Test 74: Unsupported data format, using INT
-- SAS example formatter only support String and Float types. For other types of data (like INT), it throws error "unsupported data type"

    --  Create format_long table for formatter long record test
    DROP TABLE IF EXISTS format_long CASCADE;
    CREATE TABLE format_long (
       id       int,
       name     text
    ) DISTRIBUTED by (id);

    -- Create RET and WET using demoprot and custom format UDFs
    DROP EXTERNAL TABLE IF EXISTS format_long_w;
    CREATE WRITABLE EXTERNAL TABLE format_long_w(like format_long) 
    LOCATION ('demoprot://format_long_test14') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_long_r;
    CREATE READABLE EXTERNAL TABLE format_long_r(like format_long) 
        -- using source data file format_long_test13 created by previous test
    LOCATION ('demoprot://format_long_test13')
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Now loading each record with 4000 characters, less than 4096 bytes for each record
    truncate table format_long;
    insert into format_long select 1,'oxo1';
    insert into format_long select 2,'oxo2';
    insert into format_long select 3,'oxo3';
    insert into format_long select 4,'oxo4';

    -- Write to WET
    -- insert should fail since INT type is not supported
    INSERT INTO format_long_w (SELECT * FROM format_long);
    -- ERROR:  formatter_export error: unsupported data type (gpformatter.c:100)  (seg2 rh55-qavm58:5532 pid=20668) (cdbdisp.c:1458)

    -- Read from RET using data file format_long_test13 create by previous test
    -- select should fail since INT type is not supported
    select count(*) from format_long_r;
    -- ERROR:  formatter_import error: unsupported data type (gpformatter.c:256)  (seg1 slice1 rh55-qavm57:5533 pid=20204) (cdbdisp.c:1458)
-- Test 75: External table contains dropped columns
-- SAS example formatter does NOT support dropping column from external table (both RET and WET).
-- Both import and export operations will fail. This is expected.

    --  Create example source table formatsource
    DROP TABLE IF EXISTS formatsource CASCADE;
    CREATE TABLE formatsource (
       id       float8,
       name     text, 
       value1   float8
    ) DISTRIBUTED by (id);

    -- Create RET and WET using demoprot and custom format UDFs
    DROP EXTERNAL TABLE IF EXISTS format_w;
    CREATE WRITABLE EXTERNAL TABLE format_w(like formatsource) 
    LOCATION ('demoprot://format_test15') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_r;
    CREATE READABLE EXTERNAL TABLE format_r(like formatsource) 
        -- using source data file format_long_test13 created by previous test
    LOCATION ('demoprot://format_long_test13') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Drop column value1 from source table formatsource
    ALTER TABLE formatsource DROP COLUMN value1;

    -- Drop column value1 from RET and WET
    ALTER EXTERNAL TABLE format_r DROP COLUMN value1;
    ALTER EXTERNAL TABLE format_w DROP COLUMN value1;

    -- Loading records 
    truncate table formatsource;
    insert into formatsource select 1,'oxo1';
    insert into formatsource select 2,'oxo2';
    insert into formatsource select 3,'oxo3';
    insert into formatsource select 4,'oxo4';

    -- Write to WET
    -- insert should failed because of dropped column value1
    INSERT INTO format_w (SELECT * FROM formatsource);
    -- ERROR:  formatter_export: dropped columns (gpformatter.c:80)  (seg1 rh55-qavm57:5533 pid=20454) (cdbdisp.c:1458)

    -- Read from RET using data file format_long_test13 create by previous test
    select count(*) from format_r;
    -- ERROR:  formatter_import: dropped columns (gpformatter.c:244)  (seg2 slice1 rh55-qavm58:5532 pid=20911) (cdbdisp.c:1458)
-- Test 76: External table contains added column
-- Add column to external table is fine.
-- The value of the added column would be either null or NaN, depends on the data type.

    --  Create example source table formatsource
    DROP TABLE IF EXISTS formatsource CASCADE;
    CREATE TABLE formatsource (
       id       float8,
       name     char(10)
    ) DISTRIBUTED by (id);

    -- Create RET and WET using demoprot and custom format UDFs
    DROP EXTERNAL TABLE IF EXISTS format_w;
    CREATE WRITABLE EXTERNAL TABLE format_w(like formatsource) 
    LOCATION ('demoprot://exttabtest_test76.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_r;
    CREATE READABLE EXTERNAL TABLE format_r(like formatsource) 
    LOCATION ('demoprot://exttabtest_test76.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Add column value1 to RET and WET
    ALTER EXTERNAL TABLE format_r ADD COLUMN value1 float8;
    ALTER EXTERNAL TABLE format_w ADD COLUMN value1 float8;

    -- Loading records 
    truncate table formatsource;
    insert into formatsource select 1,'oxo1';
    insert into formatsource select 2,'oxo2';
    insert into formatsource select 3,'oxo3';
    insert into formatsource select 4,'oxo4';
    insert into formatsource select 5,'oxo5';
    insert into formatsource select 6,'oxo6';
    insert into formatsource select 7,'oxo7';
    insert into formatsource select 8,'oxo8';
    insert into formatsource select 9,'oxo9';

    -- Write to WET
    -- insert should be successful
    INSERT INTO format_w (SELECT * FROM formatsource);

    -- Read from RET 
    select id, value1 from format_r order by id;

-- Test 77: data with null values
-- SAS example formatter casts null value to:
-- * null for String
-- * NaN for Float8 

    --  Create example source table formatsource
    DROP TABLE IF EXISTS formatsource CASCADE;
    CREATE TABLE formatsource (
       id       float8,
       name     text, 
       value1   float8
    ) DISTRIBUTED by (id);

    -- Create RET and WET using demoprot and custom format UDFs
    DROP EXTERNAL TABLE IF EXISTS format_w;
    CREATE WRITABLE EXTERNAL TABLE format_w(like formatsource) 
    LOCATION ('demoprot://exttabtest_test77.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_export_s');

    DROP EXTERNAL TABLE IF EXISTS format_r;
    CREATE READABLE EXTERNAL TABLE format_r(like formatsource) 
    LOCATION ('demoprot://exttabtest_test77.txt') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Loading records 
    truncate table formatsource;
    insert into formatsource values (1,null,null);
    insert into formatsource values (2,null,null);
    insert into formatsource values (3,null,null);
    insert into formatsource values (4,null,null);
    insert into formatsource values (5,null,null);
    insert into formatsource values (6,null,null);
    insert into formatsource values (7,null,null);
    insert into formatsource values (8,null,null);
    insert into formatsource values (9,null,null);

    -- Write to WET
    -- insert should be successful
    INSERT INTO format_w (SELECT * FROM formatsource);

    -- Read from RET 
    select * from format_r where name is null order by id;
-- Test 78: Read from empty data file
-- SAS example formatter can correctly handle empty input, either from /dev/null or from empty input data files.

    --  Create example source table formatsource
    DROP TABLE IF EXISTS formatsource CASCADE;
    CREATE TABLE formatsource (
       id       float8,
       name     text, 
       value1   float8
    ) DISTRIBUTED by (id);

    -- Create RET with custom format using /dev/null as source file
    DROP EXTERNAL TABLE IF EXISTS format_r;
    CREATE READABLE EXTERNAL TABLE format_r(like formatsource) 
    LOCATION ('demoprot:///dev/null') 
    FORMAT 'CUSTOM' (FORMATTER='formatter_import_s');

    -- Read from RET using /dev/null
    select * from format_r;
-- Test 81: Protocol validator - create protocol with validator

-- create the protocol read and write STABLE functions
CREATE OR REPLACE FUNCTION write_to_file() RETURNS integer AS
'$libdir/gpextprotocol.so', 'demoprot_export' LANGUAGE C STABLE;
CREATE OR REPLACE FUNCTION read_from_file() RETURNS integer AS
'$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE;

-- create validation STABLE function
CREATE OR REPLACE FUNCTION url_validator() RETURNS void AS
   '$libdir/gpextprotocol.so', 'demoprot_validate_urls' LANGUAGE C STABLE;

-- declare the protocol name along with in/out funcs and validator func
DROP PROTOCOL IF EXISTS demoprot CASCADE;
CREATE PROTOCOL demoprot (
    readfunc  = read_from_file, 
    writefunc = write_to_file,
    validatorfunc = url_validator
);
    -- Test 82: At ext table create time, validate number of URLs cannot exceed number of primary segments
    CREATE READABLE EXTERNAL TABLE exttabtest_5url_r(like exttabtest)
        LOCATION('demoprot://test1.txt', 
                 'demoprot://test2.txt',
                 'demoprot://test3.txt',
                 'demoprot://test4.txt',
                 'demoprot://test5.txt') 
    FORMAT 'text';
    -- ERROR:  more than 2 urls aren't allowed in this protocol
    -- Test 83: At ext table create time, url string cannot contain "secured_directory"
    CREATE READABLE EXTERNAL TABLE exttabtest_3url_r(like exttabtest)
        LOCATION('demoprot://test1.txt', 
                 'demoprot://secured_directory/test2.txt') 
    FORMAT 'text';
    -- ERROR:  using 'secured_directory' in a url isn't allowed
-- Test 4: Negative - validator protocol function must return void

-- create the validator function and returns integer, which is invalid
DROP FUNCTION IF EXISTS url_validator();
CREATE OR REPLACE FUNCTION url_validator() RETURNS integer AS
   '$libdir/gpextprotocol.so', 'demoprot_validate_urls' LANGUAGE C STABLE;

-- declare the protocol name along with in/out funcs and validator func
DROP PROTOCOL IF EXISTS demoprot CASCADE;
CREATE PROTOCOL demoprot (
readfunc = read_from_file,
writefunc = write_to_file,
validatorfunc = url_validator
);
-- Test 5: Negative - invalid protocol attribute name for validator function: must be "validatorfunc"
-- declare the protocol using invalid attribute name "validatorproc"

    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc  = read_from_file, 
        writefunc = write_to_file,
        validatorproc = url_validator
    );

-- ERROR: protocol attribute "validatorproc" not recognized

-- ERROR: using 'secured_directory' in a url isn't allowed
-- Create multiple roles with login option so that they can be used for protocol permission tests and alter protocol tests

    -- Create another suerpuer user demoprot_super
    drop role if exists demoprot_super;
    create role demoprot_super with SUPERUSER LOGIN;

    -- Create a non-privileged user demoprot_nopriv
    drop role if exists demoprot_nopriv;
    create role demoprot_nopriv with login ;

    -- Create a gphdfs_user with CREATEEXTTABLE privilege using gphdfs protocol
    drop role if exists gphdfs_user;
    create role gphdfs_user with login CREATEEXTTABLE (protocol='gphdfs');
    -- WARNING:  GRANT/REVOKE on gphdfs is deprecated
    -- HINT:  Issue the GRANT or REVOKE on the protocol itself
    -- NOTICE:  resource queue required -- using default resource queue "pg_default"
    -- CREATE ROLE


-- Test 92: Rename existing protocol
    DROP FUNCTION IF EXISTS url_validator();

    CREATE OR REPLACE FUNCTION url_validator() RETURNS void AS
            '$libdir/gpextprotocol.so', 'demoprot_validate_urls' LANGUAGE C STABLE;

    CREATE PROTOCOL demoprot (
        readfunc  = read_from_file, 
        writefunc = write_to_file,
        validatorfunc = url_validator
    );

    -- Create external RET and WET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r;
    CREATE READABLE EXTERNAL TABLE exttabtest_r(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text';

    -- write to WET
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w (SELECT * FROM exttabtest);

    -- Rename existing protocol
    ALTER PROTOCOL demoprot RENAME to demoprot_new;

    -- checking pg_extprotocol
    select ptcname, ptctrusted 
    from pg_extprotocol 
    where ptcname like 'demoprot%' order by ptcname;

    -- Check existing ext table that created still refer to old protocol name
    \d exttabtest_r

    -- Create a new ext table using the new protocol name
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_new;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new(like exttabtest)
        LOCATION('demoprot_new://exttabtest.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_new;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new(like exttabtest)
        LOCATION('demoprot_new://exttabtest.txt') 
    FORMAT 'text';

    -- Verify access old ext table that referencing old protocol name would fail
    -- This is expected.
    select * from exttabtest_r;
    -- ERROR:  protocol "demoprot" does not exist  (seg1 slice1 rh55-qavm57:5533 pid=8558)

    -- Verify access new ext table would be successful
    -- However demoprot implementation prevents using any other protocol name than "demoprot"
    -- therefore the error is expected.
    select count(*) from exttabtest_r_new;

    -- Rename protocol name back to demoprot
    ALTER PROTOCOL demoprot_new RENAME to demoprot;

    -- Verify access old ext table that referencing old protocol name would succeed
    select count(*) from exttabtest_r;

-- Test 93: Trusted protocol - Change ownership
-- The owner of trusted protocol (not a superuser) can create external table
-- using the protocol, even without SELECT or INSERT permission granted

    -- login as superuser huangh5
    -- create trusted protocol demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE TRUSTED PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- Check the owner of trusted protocl demoprot is current user
    select ptcname, ptctrusted 
    from pg_extprotocol join pg_user 
    on pg_extprotocol.ptcowner=pg_user.usesysid
    where ptcname='demoprot' 
    and usename=(select user);

    -- Change protocol demoprot owner to non-privileged user "demoprot_nopriv"
    ALTER PROTOCOL demoprot OWNER TO demoprot_nopriv;

    -- Drop the existing external RET and WET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_new;
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_new;

    -- Check the owner of demoprot is demoprot_nopriv
    -- and no protocol permission has been granted
    select ptcname, ptctrusted,ptcacl, usename 
    from pg_extprotocol join pg_user 
    on pg_extprotocol.ptcowner=pg_user.usesysid
    where ptcname='demoprot';

    -- Check a dependency entry is added into pg_shdepend table
    select count(*)
    from pg_shdepend, pg_extprotocol, pg_user 
    where pg_extprotocol.ptcowner=pg_user.usesysid 
    and pg_shdepend.objid=pg_extprotocol.oid 
    and pg_extprotocol.ptcname='demoprot' 
    and pg_user.usename='demoprot_nopriv';


    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- As superuser, REVOKE ALL privileges on protocol from owner demoprot_nopriv
    REVOKE ALL ON PROTOCOL demoprot FROM demoprot_nopriv;

    -- connect as non-privileged user "demoprot_nopriv"
    -- which is the owner of trusted demoprot 
    SET ROLE demoprot_nopriv;
    select user;

   -- Verify that even though no permission has been 
    -- granted to non-privileged user "demoprot_nopriv",
    -- this user can still create new ext tables 
    -- using trusted protocol demoprot, because the user 
    -- is the owner of the protocol demoprot.
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new(like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- Verify non-privileged user "demoprot_nopriv" can export data via new created WET exttabtest_w_new
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w_new (SELECT * FROM exttabtest);

    -- Verify non-privileged user "demoprot_nopriv" can load data via new created RET exttabtest_r_new
    select count(*) from exttabtest_r_new;


    -- Verified owner (non superuser) can drop the protocol
    DROP PROTOCOL demoprot CASCADE;

RESET ROLE;

-- Test 94: Untrusted protocol - Change ownership
-- The owner of untrusted protocol (not a superuser) can still create external table
-- using the untrusted protocol.
    -- connect as superuser 
    -- create untrusted protocol demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- Try to change protocol demoprot owner to non-privileged user "demoprot_nopriv"
    -- Should get: ERROR:  untrusted protocol "demoprot" can't be owned by non superuser
    -- Therefore the owner of trusted protocl demoprot is still the current superuser
    ALTER PROTOCOL demoprot OWNER TO demoprot_nopriv;

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- As superuser, REVOKE ALL privileges on protocol from owner demoprot_nopriv
    -- The error is correctly shown 
    REVOKE ALL ON PROTOCOL demoprot FROM demoprot_nopriv;
    -- ERROR:  protocol "demoprot" is not trusted

    -- login as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Verify that no permission has been 
    -- granted to non-privileged user "demoprot_nopriv",
    -- this user can cannot create new ext tables 
    -- using untrusted protocol demoprot.
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new2 (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new2 (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- Verified non superuser cannot drop the protocol
    DROP PROTOCOL demoprot;
-- Test 95: Alter protocol negative tests
    ALTER PROTOCOL demoprot (
        readfunc = read_from_file_immutable,
        writefunc = write_to_file_immutable
    );
--    ERROR:  syntax error at or near "("

    ALTER PROTOCOL demoprot update readfunc = read_from_file_immutable;
--    ERROR:  syntax error at or near "update"

    ALTER PROTOCOL demoprot trusted;
--    ERROR:  syntax error at or near "trusted"
RESET ROLE;
-- Test 96: Untrusted protocol - Superuser
-- Non-owner superuser does not have any limitation when using non-trusted protocol.

    -- login as superuser huangh5
    -- create untrusted protocol demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- Check the owner of trusted protocl demoprot is current superuser
    select ptcname, ptctrusted 
    from pg_extprotocol join pg_user 
    on pg_extprotocol.ptcowner=pg_user.usesysid
    where ptcname='demoprot' 
    and usename=(select user);

    -- connect as a different superuser "demoprot_super"
    SET ROLE demoprot_super;
    select user;
    
    SELECT * FROM clean_exttabtest_files;
    -- Verify superuser "demoprot_super" can create new ext table using untrusted protocol demoprot
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_new;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new(like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_new;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new(like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- Verify superuser demoprot_super can still export data via new created WET exttabtest_w_new
    INSERT INTO exttabtest_w_new (SELECT * FROM exttabtest);

    -- Verify superuser demoprot_super can access new created RET exttabtest_r_new
    select count(*) from exttabtest_r_new;
RESET ROLE;
  
-- Test 97: Non-trusted protocol - non-priv user
-- Non-privileged user cannot use non-trusted protocol to create external table.
-- With granted permissions on existing external table, Non-privileged user can access existing WET and RET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r;
    CREATE READABLE EXTERNAL TABLE exttabtest_r(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text';

    DROP EXTERNAL TABLE IF EXISTS exttabtest_w;
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w(like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- As superuser, GRANT SELECT permission on RET
    -- and INSERT permission on WET to on-privileged user "demoprot_nopriv"
    -- to non-privileged user "demoprot_nopriv"
    GRANT SELECT ON exttabtest_r TO demoprot_nopriv;
    GRANT INSERT ON exttabtest_w TO demoprot_nopriv;

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- connect as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Check current user has the SELECT permission on 'exttabtest_r'
    select has_table_privilege('exttabtest_r','select');

    -- Check current user has the INSERT permission on 'exttabtest_w'
    select has_table_privilege('exttabtest_w','insert');

    -- Check current user has the SELECT permission on 'exttabtest'
    select has_table_privilege('exttabtest','select');

    -- Verify with INSERT permission granted, non-privileged user "demoprot_nopriv" 
    -- can export data via WET that was created using non-trusted protocol
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w (SELECT * FROM exttabtest);

    -- Verify with SELECT permission granted, non-privileged user "demoprot_nopriv" 
    -- can load data via RET that was created using non-trusted protocol
    select count(*) from exttabtest_r;

    -- Verify non-privileged user "demoprot_nopriv" cannot create new ext table 
    -- using untrusted protocol demoprot
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_new2;
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new2 (like exttabtest)
        LOCATION('demoprot://exttabtest.txt') 
    FORMAT 'text';

-- Test 98: Trusted Protocol - Negative Tests
    -- create protocol using incorrect keyword TRUST instead of TRUSTED
    DROP PROTOCOL IF EXISTS demoprot_trusted_bad;
    CREATE TRUST PROTOCOL demoprot_trusted_bad (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- create protocol using incorrect keyword UNTRUSTED
    DROP PROTOCOL IF EXISTS demoprot_trusted_bad;
    CREATE UNTRUSTED PROTOCOL demoprot_trusted_bad (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- create protocol using TRUSTED at wrong position
    DROP PROTOCOL IF EXISTS demoprot_trusted_bad;
    CREATE PROTOCOL TRUSTED demoprot_trusted_bad (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- create protocol using TRUSTED at wrong position
    DROP PROTOCOL IF EXISTS demoprot_trusted_bad;
    CREATE PROTOCOL demoprot_trusted_bad TRUSTED (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );
RESET ROLE;
-- Test 99: Trusted protocol - Grant All
-- Grant all permissions ON trusted protocol to non-privileged user
-- Non-privileged user can use trusted protocol to create external table.

    -- connect as superuser 
    -- create trusted protocol demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE TRUSTED PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- Drop existing WET and RET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_new;
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_new;

    -- As superuser, GRANT ALL privileges on protocol to non-privileged user demoprot_nopriv
    GRANT ALL ON PROTOCOL demoprot TO demoprot_nopriv;

    -- login as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Verify non-privileged user "demoprot_nopriv" can create new ext table 
    -- using trusted protocol demoprot
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- Verify non-privileged user "demoprot_nopriv" can export data via new created WET exttabtest_w_new
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w_new (SELECT * FROM exttabtest);

    -- Verify non-privileged user "demoprot_nopriv" can load data via new created RET exttabtest_r_new
    select count(*) from exttabtest_r_new;

RESET ROLE;
-- Test 99a: Trusted protocol - Revoke All
-- Revoke all permissions ON trusted protocol from non-privileged user

    -- connect as superuser 
    -- create trusted protocol demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE TRUSTED PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- As superuser, REVOKE ALL privileges on protocol from non-privileged user demoprot_nopriv
    -- Both SELECT and INSERT permissions are revoked
    REVOKE ALL ON PROTOCOL demoprot FROM demoprot_nopriv;

    -- login as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Verify after permissions have been revoked
    -- non-privileged user "demoprot_nopriv" cannot create new ext table 
    -- using the trusted protocol demoprot
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

RESET ROLE;
-- Test 101: Trusted protocol - Grant Select
-- Grant SELECT permission ON trusted protocol to non-privileged user
-- Non-privileged user can use trusted protocol to create readable external table.

    -- Create exttabtest_new.txt data file
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w (SELECT * FROM exttabtest);

    -- As superuser, demoport is created as a trusted readonly protocol
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE TRUSTED PROTOCOL demoprot (
        readfunc = read_from_file_stable
    );

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- Drop existing WET and RET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_new;
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_new;

    -- As superuser, GRANT SELECT permission on read protocol to non-privileged user demoprot_nopriv
    GRANT SELECT ON PROTOCOL demoprot TO demoprot_nopriv;

    -- login as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Verify non-privileged user "demoprot_nopriv" can create new readable ext table 
    -- using trusted protocol demoprot
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

    -- Verify non-privileged user "demoprot_nopriv" cannot create new writable ext table 
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- Verify non-privileged user "demoprot_nopriv" can load data via new created RET exttabtest_r_new
    select count(*) from exttabtest_r_new;

RESET ROLE;
-- Test 102: Trusted protocol - Revoke Select
-- Revoke SELECT permission ON trusted protocol from non-privileged user

    -- connect as superuser 
    -- create trusted protocol demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE TRUSTED PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- As superuser, REVOKE SElECT privilege on protocol from non-privileged user demoprot_nopriv
    REVOKE SELECT ON PROTOCOL demoprot FROM demoprot_nopriv;

    -- login as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Verify after SELECT permission has been revoked
    -- non-privileged user "demoprot_nopriv" cannot create new readable ext table 
    -- using the trusted protocol demoprot
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new2 (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

RESET ROLE;
-- Test 103: Trusted protocol - Grant Insert
-- Grant INSERT permission ON trusted protocol to non-privileged user
-- Non-privileged user can use trusted protocol to create writable external table.

    -- As superuser, demoport is created as a trusted readonly protocol
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE TRUSTED PROTOCOL demoprot (
        writefunc = write_to_file_stable
    );

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- Drop existing WET and RET
    DROP EXTERNAL TABLE IF EXISTS exttabtest_r_new;
    DROP EXTERNAL TABLE IF EXISTS exttabtest_w_new;

    -- As superuser, GRANT INSERT permission on read protocol to non-privileged user demoprot_nopriv
    GRANT INSERT ON PROTOCOL demoprot TO demoprot_nopriv;

    -- login as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Verify non-privileged user "demoprot_nopriv" cannot create new readable ext table 
    -- using trusted protocol demoprot
    CREATE READABLE EXTERNAL TABLE exttabtest_r_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text';

    -- Verify non-privileged user "demoprot_nopriv" can create new writable ext table 
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

    -- Verify non-privileged user "demoprot_nopriv" can export data via new created WET exttabtest_w_new
    SELECT * FROM clean_exttabtest_files;
    INSERT INTO exttabtest_w_new (SELECT * FROM exttabtest);
RESET ROLE;
-- Test 104: Trusted protocol - Revoke Insert
-- Revoke INSERT permission ON trusted protocol from non-privileged user

    -- connect as superuser 
    -- create trusted protocol demoprot
    DROP PROTOCOL IF EXISTS demoprot CASCADE;
    CREATE TRUSTED PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- As superuser, GRANT SELECT permission on heap table "exttabtest"
    -- to non-privileged user "demoprot_nopriv" so that this user
    -- can try to create external table using format: (like exttabtest)
    GRANT SELECT ON exttabtest TO demoprot_nopriv;

    -- As superuser, REVOKE INSERT privilege on protocol from non-privileged user demoprot_nopriv
    REVOKE INSERT ON PROTOCOL demoprot FROM demoprot_nopriv;

    -- login as non-privileged user "demoprot_nopriv"
    SET ROLE demoprot_nopriv;
    select user;

    -- Verify after INSERT permission has been revoked
    -- non-privileged user "demoprot_nopriv" cannot create new writable ext table 
    -- using the trusted protocol demoprot
    CREATE WRITABLE EXTERNAL TABLE exttabtest_w_new2 (like exttabtest)
        LOCATION('demoprot://exttabtest_new.txt') 
    FORMAT 'text'
    DISTRIBUTED BY (id);

RESET ROLE;
