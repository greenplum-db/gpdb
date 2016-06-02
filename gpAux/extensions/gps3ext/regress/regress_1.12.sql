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

-- ========

drop external table s3example;
create READABLE external table s3example (date text, time text, open float, high float,
        low float, volume int) location('s3://s3-ap-northeast-1.amazonaws.com/apnortheast1.s3test.pivotal.io config=/home/gpadmin/s3.conf') FORMAT 'csv';

\d s3example

SELECT sum(open) FROM s3example;

-- ========

drop external table s3example;
create READABLE external table s3example (date text, time text, open float, high float,
        low float, volume int) location('s3://s3-eu-central-1.amazonaws.com/eucentral1.s3test.pivotal.io/ config=/home/gpadmin/s3.conf') FORMAT 'csv';

\d s3example

SELECT sum(open) FROM s3example;

-- ========

drop external table s3example;
create READABLE external table s3example (date text, time text, open float, high float,
        low float, volume int) location('s3://s3-sa-east-1.amazonaws.com/saeast1.s3test.pivotal.io/ config=/home/gpadmin/s3.conf') FORMAT 'csv';

\d s3example

SELECT sum(open) FROM s3example;

-- =======
-- CLEANUP
-- =======
DROP EXTERNAL TABLE s3example;

DROP PROTOCOL s3;
