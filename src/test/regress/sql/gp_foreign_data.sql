--
-- Test foreign-data wrapper and server management. Greenplum MPP specific
--

-- start_ignore
DROP FOREIGN DATA WRAPPER dummy CASCADE;
-- end_ignore

CREATE FOREIGN DATA WRAPPER dummy;
COMMENT ON FOREIGN DATA WRAPPER dummy IS 'useless';

-- CREATE FOREIGN TABLE
CREATE SERVER s0 FOREIGN DATA WRAPPER dummy;
CREATE FOREIGN TABLE ft2 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',', mpp_execute 'a');           -- ERROR
CREATE FOREIGN TABLE ft2 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',', mpp_execute 'any');
\d+ ft2
CREATE FOREIGN TABLE ft3 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',', mpp_execute 'master');
CREATE FOREIGN TABLE ft4 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',', mpp_execute 'all segments');

CREATE FOREIGN TABLE ft5 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',', mpp_execute 'all segments')
DISTRIBUTED BY (c1);

\d+ ft5

-- Hash distribution implies mpp_execute 'all segments'
CREATE FOREIGN TABLE ft6 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',')
DISTRIBUTED BY (c1);
\d+ ft6

-- Hash distribution implies mpp_execute 'all segments', override 'any'
CREATE FOREIGN TABLE ft7 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',', mpp_execute 'any')
DISTRIBUTED BY (c1);
\d+ ft7

-- Hash distribution implies mpp_execute 'all segments', override 'master'
CREATE FOREIGN TABLE ft8 (
	c1 int
) SERVER s0 OPTIONS (delimiter ',', mpp_execute 'master')
DISTRIBUTED BY (c1);
\d+ ft8

-- Distribution policy for pxf_fdw is disabled
CREATE FOREIGN DATA WRAPPER pxf_fdw;
CREATE SERVER s1 FOREIGN DATA WRAPPER pxf_fdw;
CREATE FOREIGN TABLE ft9 (
	c1 int
) SERVER s1 OPTIONS (delimiter ',', mpp_execute 'all segments')
DISTRIBUTED BY (c1);

-- start_ignore
DROP FOREIGN DATA WRAPPER pxf_fdw CASCADE;
-- end_ignore