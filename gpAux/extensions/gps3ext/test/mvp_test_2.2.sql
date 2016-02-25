-- ========
-- PROTOCOL
-- ========

-- create the database functions
CREATE OR REPLACE FUNCTION read_from_s3() RETURNS integer AS
        '$libdir/gps3ext.so', 's3_import' LANGUAGE C STABLE;

-- declare the protocol name along with in/out funcs
CREATE PROTOCOL s3 (
        readfunc  = read_from_s3
);

-- Check out the catalog table
select * from pg_extprotocol;

drop external table s3example;
create READABLE external table s3example (date text, time text, open float, high float,
	low float, volume int) location('s3://neverland.amazonaws.com/wherever/smalldata config=/home/gpadmin/s3.conf') format 'csv';

SELECT count(*) FROM s3example;

-- =======
-- CLEANUP
-- =======
DROP EXTERNAL TABLE s3example;

DROP PROTOCOL s3;
