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

-- Test num_segments option
CREATE SERVER s1 FOREIGN DATA WRAPPER dummy OPTIONS (num_segments '3');
CREATE FOREIGN TABLE ft5 (
       c1 int
) SERVER s1 OPTIONS (delimiter ',', mpp_execute 'all segments', num_segments '5');
\d+ ft5
