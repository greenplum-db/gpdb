--
-- Test: cpuset cannot be specified when group is disabled.
--

-- start_ignore
DROP RESOURCE GROUP resource_group1;
-- end_ignore

CREATE RESOURCE GROUP resource_group1 WITH (memory_limit=5, cpuset='0');
CREATE RESOURCE GROUP resource_group1 WITH (memory_limit=5, cpu_rate_limit=5);
ALTER RESOURCE GROUP resource_group1 SET cpuset '0';

DROP RESOURCE GROUP resource_group1;
