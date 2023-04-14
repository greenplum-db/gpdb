--
-- Test foreign-data wrapper and server management. Greenplum MPP specific
--

-- start_ignore
DROP SERVER s0 CASCADE;
DROP SERVER s1 CASCADE;
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

-- CREATE FOREIGN SERVER WITH num_segments
CREATE SERVER s1 FOREIGN DATA WRAPPER dummy OPTIONS (num_segments '5');

-- CHECK FOREIGN SERVER's OPTIONS
SELECT srvoptions FROM pg_foreign_server WHERE srvname = 's1';

-- start_ignore
DROP FOREIGN DATA WRAPPER dummy CASCADE;
-- end_ignore
