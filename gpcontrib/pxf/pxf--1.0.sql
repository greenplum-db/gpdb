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

CREATE OR REPLACE FUNCTION pg_catalog.pxfwritable_import() RETURNS record
AS '$libdir/pxf.so', 'gpdbwritableformatter_import'
LANGUAGE C STABLE;

CREATE OR REPLACE FUNCTION pg_catalog.pxfwritable_export(record) RETURNS bytea
AS '$libdir/pxf.so', 'gpdbwritableformatter_export'
LANGUAGE C STABLE;

CREATE TRUSTED PROTOCOL pxf (
  writefunc     = pxf_write,
  readfunc      = pxf_read,
  validatorfunc = pxf_validate);