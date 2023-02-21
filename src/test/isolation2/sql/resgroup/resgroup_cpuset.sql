-- start_ignore
DROP VIEW IF EXISTS busy;
DROP VIEW IF EXISTS cancel_all;
DROP TABLE IF EXISTS bigtable;
CREATE LANGUAGE plpython3u;
-- end_ignore

CREATE TABLE bigtable AS
    SELECT i AS c1, 'abc' AS c2
    FROM generate_series(1,50000) i;

CREATE VIEW busy AS
    SELECT count(*)
    FROM
    bigtable t1,
    bigtable t2,
    bigtable t3,
    bigtable t4,
    bigtable t5
    WHERE 0 = (t1.c1 % 2 + 10000)!
      AND 0 = (t2.c1 % 2 + 10000)!
      AND 0 = (t3.c1 % 2 + 10000)!
      AND 0 = (t4.c1 % 2 + 10000)!
      AND 0 = (t5.c1 % 2 + 10000)!
    ;

CREATE VIEW cancel_all AS
    SELECT pg_cancel_backend(pid)
    FROM pg_stat_activity
    WHERE query LIKE 'SELECT * FROM busy%';

CREATE RESOURCE GROUP rg1_cpuset_test WITH (cpuset='0');
CREATE ROLE role1_cpuset_test RESOURCE GROUP rg1_cpuset_test;

GRANT ALL ON busy TO role1_cpuset_test;

-- we suppose core 0 & 1 are available

10: SET ROLE TO role1_cpuset_test;
10: BEGIN;
10&: SELECT * FROM busy;

select pg_sleep(2);

11: BEGIN;
11: SELECT check_cpuset('rg1_cpuset_test', '0');

ALTER RESOURCE GROUP rg1_cpuset_test SET cpuset '1';
select pg_sleep(2);

11: SELECT check_cpuset('rg1_cpuset_test', '1');

ALTER RESOURCE GROUP rg1_cpuset_test SET cpuset '0,1';
select pg_sleep(2);

11: SELECT check_cpuset('rg1_cpuset_test', '0,1');
11: END;

-- change to cpu_hard_quota_limit while the transaction is running
ALTER RESOURCE GROUP rg1_cpuset_test SET cpu_hard_quota_limit 70;

-- cancel the transaction
-- start_ignore
select * from cancel_all;

10<:
10q:
11q:
-- end_ignore

-- test whether the cpu_hard_quota_limit had taken effect
10: SET ROLE TO role1_cpuset_test;
10: BEGIN;
10&: SELECT * FROM busy;

select pg_sleep(2);

11: BEGIN;
11: SELECT check_cpuset('rg1_cpuset_test', '');

-- cancel the transaction
-- start_ignore
select * from cancel_all;

10<:
10q:
11q:
-- end_ignore

-- test cpu_usage
10: SET ROLE TO role1_cpuset_test;
10: BEGIN;
10&: SELECT * FROM busy;

select pg_sleep(2);

11: BEGIN;
11: select (cpu_usage::json->>'0')::float > 50 from gp_toolkit.gp_resgroup_status where rsgname='rg1_cpuset_test';
-- cancel the transaction
-- start_ignore
select * from cancel_all;

10<:
10q:
11q:
-- end_ignore

-- positive: cgroup cpuset must correspond to config cpuset
-- default group value must be valid
-- suppose the cores numbered 0 & 1 are available
SELECT check_cpuset_rules();
CREATE RESOURCE GROUP rg1_test_group WITH (cpuset='0');
SELECT check_cpuset_rules();
CREATE RESOURCE GROUP rg2_test_group WITH (cpuset='1');
SELECT check_cpuset_rules();
ALTER RESOURCE GROUP rg1_test_group SET cpu_hard_quota_limit 1;
SELECT check_cpuset_rules();
ALTER RESOURCE GROUP rg1_test_group SET cpuset '0';
SELECT check_cpuset_rules();
ALTER RESOURCE GROUP rg1_test_group SET cpu_hard_quota_limit 1;
SELECT check_cpuset_rules();
DROP RESOURCE GROUP rg1_test_group;
SELECT check_cpuset_rules();
DROP RESOURCE GROUP rg2_test_group;
SELECT check_cpuset_rules();
-- positive: create a resource group contains all cpu core
-- the minimum numbered core left in default cpuset group
SELECT create_allcores_group('rg1_test_group');
SELECT check_cpuset_rules();
DROP RESOURCE GROUP rg1_test_group;
SELECT check_cpuset_rules();
-- negative: simulate DDL fail
-- create fail
SELECT gp_inject_fault('create_resource_group_fail', 'error', 1);
CREATE RESOURCE GROUP rg1_test_group WITH (cpuset='0');
SELECT groupid, groupname, cpuset
	FROM gp_toolkit.gp_resgroup_config
	WHERE cpuset != '-1';
SELECT check_cpuset_rules();
SELECT gp_inject_fault('create_resource_group_fail', 'reset', 1);
-- start_ignore
DROP RESOURCE GROUP rg1_test_group;
-- end_ignore

-- test segment/master cpuset
CREATE RESOURCE GROUP rg_multi_cpuset1 WITH (concurrency=2, cpuset='0;0');
ALTER RESOURCE GROUP rg_multi_cpuset1 set CPUSET '1;1';
select groupname,cpuset from gp_toolkit.gp_resgroup_config where groupname='rg_multi_cpuset1';

DROP RESOURCE GROUP rg_multi_cpuset1;

REVOKE ALL ON busy FROM role1_cpuset_test;
DROP ROLE role1_cpuset_test;
DROP RESOURCE GROUP rg1_cpuset_test;
DROP FUNCTION check_cpuset_rules();
DROP FUNCTION check_cpuset(TEXT, TEXT);
DROP FUNCTION create_allcores_group(TEXT);
DROP VIEW cancel_all;
DROP VIEW busy;
DROP TABLE bigtable;
