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

CREATE TABLE stock (date text, time text, open float, high float, low float, volume int) DISTRIBUTED BY (date) PARTITION BY RANGE (volume)
(
	PARTITION stock10000 START (10000) INCLUSIVE,
	PARTITION stock20000 START (20000) INCLUSIVE,
	PARTITION stock30000 START (30000) INCLUSIVE
);

INSERT INTO stock (date, time, open, high, low, volume) VALUES (03/08/2011, 094022, 35.25, 35.24, 35.29, 20001);

create READABLE external table ext_stock000 (date text, time text, open float, high float,
	low float, volume int) location('s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal config=/home/gpadmin/s3.conf') FORMAT 'csv';

ALTER TABLE stock ADD PARTITION stock000 START (0) INCLUSIVE END (10000) EXCLUSIVE;

ALTER TABLE stock EXCHANGE PARTITION FOR (0) WITH TABLE ext_stock000 WITHOUT VALIDATION;

SELECT count(*) FROM stock_1_prt_stock000;
SELECT count(*) FROM stock;

-- =======
-- CLEANUP
-- =======
DROP TABLE ext_stock000;
DROP TABLE stock;

DROP PROTOCOL s3;
