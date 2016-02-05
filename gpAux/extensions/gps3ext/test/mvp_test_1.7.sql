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
-- create READABLE external table s3example (Year text, Month text, DayofMonth text, DayOfWeek text, DepTime text, CRSDepTime text, ArrTime text,CRSArrTime text, UniqueCarrier text, FlightNum text,TailNum text, ActualElapsedTime text, CRSElapsedTime text, AirTime text, ArrDelay text, DepDelay text, Origin text, Dest text, Distance text, TaxiIn text, TaxiOut text, Cancelled text, CancellationCode text, Diverted text, CarrierDelay text, WeatherDelay text, NASDelay text, SecurityDelay text, LateAircraftDelay text) location('s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/hugefile config=/home/gpadmin/s3.conf') format 'csv' LOG ERRORS SEGMENT REJECT LIMIT 100 PERCENT;
create READABLE external table s3example (Year text, Month text, DayofMonth text, DayOfWeek text, DepTime text, CRSDepTime text, ArrTime text,CRSArrTime text, UniqueCarrier text, FlightNum text,TailNum text, ActualElapsedTime text, CRSElapsedTime text, AirTime text, ArrDelay text, DepDelay text, Origin text, Dest text, Distance text, TaxiIn text, TaxiOut text, Cancelled text, CancellationCode text, Diverted text, CarrierDelay text, WeatherDelay text, NASDelay text, SecurityDelay text, LateAircraftDelay text) location('s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/hugefile config=/home/gpadmin/s3.conf') format 'csv' SEGMENT REJECT LIMIT 100 PERCENT;

SELECT count(*) FROM s3example;

-- =======
-- CLEANUP
-- =======
DROP EXTERNAL TABLE s3example;

DROP PROTOCOL s3;
