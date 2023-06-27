-- positive: io limit with correct syntax
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000');
SELECT io_limit FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_test_group';
DROP RESOURCE GROUP rg_test_group;

-- with space
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit=' pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000');
DROP RESOURCE GROUP rg_test_group;

-- with space
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000, wbps=1000,riops=1000,wiops=1000');
DROP RESOURCE GROUP rg_test_group;

-- with *
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='*:rbps=1000,wbps=1000,riops=1000,wiops=1000');
DROP RESOURCE GROUP rg_test_group;

-- negative: io limit with incorrect syntax
-- * must be unique tablespace
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000;*:wbps=1000');

-- tail ;
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,wiops=1000;');

-- tail ,
CREATE RESOURCE GROUP rg_test_group WITH (concurrency=10, cpu_max_percent=10, io_limit='pg_default:rbps=1000,wbps=1000,riops=1000,');

