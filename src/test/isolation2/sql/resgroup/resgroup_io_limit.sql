-- positive: io limit with correct syntax
CREATE RESOURCE GROUP rg_test_group1 WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000');
SELECT io_limit FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group1';

SELECT check_cgroup_io_max('rg_test_group1', 'pg_default', 'rbps=1048576000 wbps=1048576000 riops=1000 wiops=1000');

ALTER RESOURCE GROUP rg_test_group1 SET io_limit 'pg_default:rbps=1000,wbps=1000';

SELECT check_cgroup_io_max('rg_test_group1', 'pg_default', 'rbps=1048576000 wbps=1048576000 riops=max wiops=max');

-- with space
CREATE RESOURCE GROUP rg_test_group2 WITH (concurrency=10, cpu_max_percent=10, io_limit=' pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000');

-- with space
CREATE RESOURCE GROUP rg_test_group3 WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000, wbps=1000,riops=1000,wiops=1000');

-- with *
CREATE RESOURCE GROUP rg_test_group4 WITH (concurrency=10, cpu_max_percent=10, io_limit='*:rbps=1000,wbps=1000,riops=1000,wiops=1000');

SELECT check_cgroup_io_max('rg_test_group4', 'pg_default', 'rbps=1048576000 wbps=1048576000 riops=1000 wiops=1000');

-- negative: io limit with incorrect syntax
-- * must be unique tablespace
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000;*:wbps=1000');

-- tail ;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000;');

-- tail ,
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,');

-- wrong key
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rrbps=1000');

-- wrong tablespace name
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pgdefault:rbps=1000,wbps=1000,riops=1000');

-- clean
DROP RESOURCE GROUP rg_test_group1;
DROP RESOURCE GROUP rg_test_group2;
DROP RESOURCE GROUP rg_test_group3;
DROP RESOURCE GROUP rg_test_group4;
