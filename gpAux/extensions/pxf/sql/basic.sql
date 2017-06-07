------------------------------------------------------------------
-- PXF Protocol/Formatters
------------------------------------------------------------------
CREATE OR REPLACE FUNCTION pg_catalog.pxf_write() RETURNS integer
AS '$libdir/pxf.so', 'pxfprotocol_export'
LANGUAGE C STABLE;

CREATE OR REPLACE FUNCTION pg_catalog.pxf_read() RETURNS integer
AS '$libdir/pxf.so', 'pxfprotocol_import'
LANGUAGE C STABLE;

CREATE OR REPLACE FUNCTION pg_catalog.pxf_validate() RETURNS void
AS '$libdir/pxf.so', 'pxfprotocol_validate_urls'
LANGUAGE C STABLE;

CREATE TRUSTED PROTOCOL pxf (
  writefunc     = pxf_write,
  readfunc      = pxf_read,
  validatorfunc = pxf_validate); 

CREATE EXTERNAL TABLE pxf_read_test (a INT, b TEXT)
LOCATION ('pxf://namenode:51200/data/pxf_hdfs_read.txt?PROFILE=TestProfile')
FORMAT 'TEXT' (DELIMITER ',');

SELECT * from pxf_read_test order by a;