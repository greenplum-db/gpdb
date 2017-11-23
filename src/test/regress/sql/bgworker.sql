--
-- Tests the Greenplum specific bgworker framework can launch
-- processes registed in contrib/bgworker_example
--

-- start_ignore
SELECT * FROM pg_sleep(3);
DROP EXTERNAL WEB TABLE IF EXISTS find_bgworker CASCADE;
CREATE EXTERNAL WEB TABLE find_bgworker(line text) EXECUTE 'ps aux | grep bgworker 2>&1' ON MASTER FORMAT 'TEXT';

CREATE OR REPLACE VIEW check_number_bgworkers AS
SELECT CASE WHEN count(*) = (3 * (SELECT count(*) AS segnum FROM gp_segment_configuration WHERE role = 'p' AND hostname IN (SELECT hostname FROM gp_segment_configuration WHERE role = 'p' AND content = -1))) THEN true ELSE false END AS ok
FROM find_bgworker WHERE line NOT LIKE '%grep%';

CREATE OR REPLACE VIEW number_bgworkers_should_be_zero AS
SELECT CASE WHEN count(*) = 0 THEN true ELSE false END AS ok
FROM find_bgworker WHERE line NOT LIKE '%grep%';
-- end_ignore

-- This checks number of started bgworker processes equals
-- (number of workers registed in contrib/bgworker_example) * (postmasters on this host)
SELECT * FROM check_number_bgworkers;
-- Should return 'true'

-- start_ignore
DROP EXTERNAL WEB TABLE IF EXISTS kill_bgworker;
CREATE EXTERNAL WEB TABLE kill_bgworker(line text) EXECUTE 'kill $(ps aux | grep bgworker | grep -v grep | grep -v kill | awk ''{print $2}'') > /dev/null 2>&1' ON MASTER FORMAT 'TEXT';
-- end_ignore

SELECT * FROM kill_bgworker;
SELECT * FROM number_bgworkers_should_be_zero;
-- Should be true

-- Wait for postmaster restart bgworker processes
SELECT * FROM pg_sleep(6);

SELECT * FROM check_number_bgworkers;
-- Should return 'true'

-- start_ignore
DROP VIEW check_number_bgworkers;
DROP VIEW number_bgworkers_should_be_zero;
DROP EXTERNAL WEB TABLE IF EXISTS find_bgworker CASCADE;
DROP EXTERNAL WEB TABLE IF EXISTS kill_bgworker CASCADE;
-- end_ignore
