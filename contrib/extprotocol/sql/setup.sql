CREATE SCHEMA exttableext;
GRANT ALL ON SCHEMA exttableext TO PUBLIC;
SET search_path TO 'exttableext';
-- Create an example table exttabtest that will be used as source table
    CREATE TABLE exttabtest(
       id       int,
       name     varchar(20),
       value1   int,
       value2   int
    ) 
    DISTRIBUTED BY (id);

-- Loading 100 records
-- Use only 100 rows for easy to verify the results
-- In order to test multiple data buffers and related edge cases, more data (at least several MBs or more)
-- will be used, as in Performance tests 1M and 100M test cases

    \echo 'loading data...'
    INSERT INTO exttabtest SELECT i, 'name'||i, i*2, i*3 FROM generate_series(1,100) i;
    ANALYZE exttabtest;

-- Test 1: create read and write functions based on example gpextprotocol.so
-- Note: Only STABLE is supported for protocol, though this has not been enforced at the time of testing

    CREATE OR REPLACE FUNCTION write_to_file_stable() RETURNS integer AS
        '$libdir/gpextprotocol.so', 'demoprot_export' LANGUAGE C STABLE;

    CREATE OR REPLACE FUNCTION read_from_file_stable() RETURNS integer AS 
        '$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE;

    -- Check pg_proc catalog table for new created functions

    SELECT proname, prolang,proisstrict,provolatile,pronargs,prorettype,prosrc,proacl FROM pg_proc 
    WHERE proname like 'write_to_file%' 
       or proname like 'read_from_file%'
    ORDER BY proname;

-- Test 2: create bi-directional protocol (read and write) using STABLE functions

    DROP PROTOCOL IF EXISTS demoprot;
    CREATE PROTOCOL demoprot (
        readfunc = read_from_file_stable,
        writefunc = write_to_file_stable
    );

    -- Check dependency: pg_depend table
    select count(*) from pg_depend 
    where objid in (
        select oid from pg_extprotocol where ptcname='demoprot');

    -- Check pg_extprotocol for new created protocol
    select extprot.ptcname, proc1.proname readfunc, proc2.proname writefunc
    from pg_extprotocol extprot, pg_proc proc1, pg_proc proc2 
    where extprot.ptcname='demoprot' 
        and extprot.ptcreadfn=proc1.oid 
        and extprot.ptcwritefn=proc2.oid;

    DROP EXTERNAL TABLE IF EXISTS clean_exttabtest_files;
    CREATE EXTERNAL WEB TABLE clean_exttabtest_files(stdout text) EXECUTE 'rm -f exttabtest*' ON ALL FORMAT 'text';
    GRANT ALL ON clean_exttabtest_files TO PUBLIC;
    SELECT * FROM clean_exttabtest_files;
