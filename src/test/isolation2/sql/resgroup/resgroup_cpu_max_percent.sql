-- start_ignore
DROP VIEW IF EXISTS cancel_all;
DROP ROLE IF EXISTS role1_cpu_test;
DROP ROLE IF EXISTS role2_cpu_test;
DROP RESOURCE GROUP rg1_cpu_test;
DROP RESOURCE GROUP rg2_cpu_test;
DROP VIEW IF EXISTS busy;
DROP TABLE IF EXISTS bigtable;

CREATE LANGUAGE plpython3u;
-- end_ignore

--
-- helper functions, tables and views
--

DROP TABLE IF EXISTS cpu_usage_samples;
CREATE TABLE cpu_usage_samples (sample text);

-- fetch_sample: select cpu_usage from gp_toolkit.gp_resgroup_status
-- and dump them into text in json format then save them in db for
-- further analysis.
CREATE OR REPLACE FUNCTION fetch_sample() RETURNS text AS $$
    import json

    group_cpus = plpy.execute('''
        SELECT groupname, cpu_usage FROM gp_toolkit.gp_resgroup_status_per_host
    ''')
    plpy.notice(group_cpus)
    json_text = json.dumps(dict([(row['groupname'], float(row['cpu_usage'])) for row in group_cpus]))
    plpy.execute('''
        INSERT INTO cpu_usage_samples VALUES ('{value}')
    '''.format(value=json_text))
    return json_text
$$ LANGUAGE plpython3u;

-- verify_cpu_usage: calculate each QE's average cpu usage using all the data in
-- the table cpu_usage_sample. And compare the average value to the expected value.
-- return true if the practical value is close to the expected value.
CREATE OR REPLACE FUNCTION verify_cpu_usage(groupname TEXT, expect_cpu_usage INT, err_rate INT)
RETURNS BOOL AS $$
    all_info = plpy.execute('''
        SELECT sample::json->'{name}' AS cpu FROM cpu_usage_samples
    '''.format(name=groupname))

    results = [float(_['cpu']) for _ in all_info]
    usage = sum(results) / len(results)

    return abs(usage - expect_cpu_usage) <= err_rate
$$ LANGUAGE plpython3u;

CREATE TABLE bigtable AS
    SELECT i AS c1, 'abc' AS c2
    FROM generate_series(1,50000) i distributed randomly;

CREATE OR REPLACE FUNCTION complex_compute(i int)
RETURNS int AS $$
    results = 1
    for j in range(1, 10000 + i):
        results = (results * j) % 35969
    return results
$$ LANGUAGE plpython3u;

CREATE VIEW busy AS
    WITH t1 as (select random(), complex_compute(c1) from bigtable),
    t2 as (select random(), complex_compute(c1) from bigtable),
    t3 as (select random(), complex_compute(c1) from bigtable),
    t4 as (select random(), complex_compute(c1) from bigtable),
    t5 as (select random(), complex_compute(c1) from bigtable)
    SELECT count(*)
    FROM
    t1, t2, t3, t4, t5;


CREATE VIEW cancel_all AS
    SELECT pg_cancel_backend(pid)
    FROM pg_stat_activity
    WHERE query LIKE 'SELECT * FROM busy%';

-- The test cases for the value of gp_resource_group_cpu_limit equals 0.9, 
-- do not change it during the test.
show gp_resource_group_cpu_limit;

-- create two resource groups
CREATE RESOURCE GROUP rg1_cpu_test WITH (concurrency=5, cpu_max_percent=-1, cpu_weight=100);
CREATE RESOURCE GROUP rg2_cpu_test WITH (concurrency=5, cpu_max_percent=-1, cpu_weight=200);

--
-- check gpdb cgroup configuration
-- The implementation of check_cgroup_configuration() is in resgroup_auxiliary_tools_*.sql
--
select check_cgroup_configuration();

-- lower admin_group's cpu_max_percent to minimize its side effect
ALTER RESOURCE GROUP admin_group SET cpu_max_percent 1;

-- create two roles and assign them to above groups
CREATE ROLE role1_cpu_test RESOURCE GROUP rg1_cpu_test;
CREATE ROLE role2_cpu_test RESOURCE GROUP rg2_cpu_test;
GRANT ALL ON FUNCTION complex_compute(int) TO role1_cpu_test;
GRANT ALL ON FUNCTION complex_compute(int) TO role2_cpu_test;
GRANT ALL ON busy TO role1_cpu_test;
GRANT ALL ON busy TO role2_cpu_test;

-- prepare parallel queries in the two groups
10: SET ROLE TO role1_cpu_test;
11: SET ROLE TO role1_cpu_test;
12: SET ROLE TO role1_cpu_test;
13: SET ROLE TO role1_cpu_test;
14: SET ROLE TO role1_cpu_test;

20: SET ROLE TO role2_cpu_test;
21: SET ROLE TO role2_cpu_test;
22: SET ROLE TO role2_cpu_test;
23: SET ROLE TO role2_cpu_test;
24: SET ROLE TO role2_cpu_test;

--
-- now we get prepared.
--
-- on empty load the cpu usage shall be 0%
--

10&: SELECT * FROM busy;
11&: SELECT * FROM busy;
12&: SELECT * FROM busy;
13&: SELECT * FROM busy;
14&: SELECT * FROM busy;

-- start_ignore
-- Gather CPU usage statistics into cpu_usage_samples
TRUNCATE TABLE cpu_usage_samples;
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
TRUNCATE TABLE cpu_usage_samples;
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
-- end_ignore

SELECT verify_cpu_usage('rg1_cpu_test', 90, 10);

-- start_ignore
SELECT * FROM cancel_all;

10<:
11<:
12<:
13<:
14<:
-- end_ignore

10q:
11q:
12q:
13q:
14q:

10: SET ROLE TO role1_cpu_test;
11: SET ROLE TO role1_cpu_test;
12: SET ROLE TO role1_cpu_test;
13: SET ROLE TO role1_cpu_test;
14: SET ROLE TO role1_cpu_test;

--
-- when there are multiple groups with parallel queries,
-- they should share the cpu usage by their cpu_weight settings,
--
-- rg1_cpu_test:rg2_cpu_test is 100:200 => 1:2, so:
--
-- - rg1_cpu_test gets 90% * 1/3 => 30%;
-- - rg2_cpu_test gets 90% * 2/3 => 60%;
--

10&: SELECT * FROM busy;
11&: SELECT * FROM busy;
12&: SELECT * FROM busy;
13&: SELECT * FROM busy;
14&: SELECT * FROM busy;

20&: SELECT * FROM busy;
21&: SELECT * FROM busy;
22&: SELECT * FROM busy;
23&: SELECT * FROM busy;
24&: SELECT * FROM busy;

-- start_ignore
TRUNCATE TABLE cpu_usage_samples;
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
TRUNCATE TABLE cpu_usage_samples;
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
SELECT fetch_sample();
SELECT pg_sleep(1.7);
-- end_ignore

SELECT verify_cpu_usage('rg1_cpu_test', 30, 10);
SELECT verify_cpu_usage('rg2_cpu_test', 60, 10);

-- start_ignore
SELECT * FROM cancel_all;

10<:
11<:
12<:
13<:
14<:

20<:
21<:
22<:
23<:
24<:

10q:
11q:
12q:
13q:
14q:


20q:
21q:
22q:
23q:
24q:
-- end_ignore



-- Test cpu max percent
ALTER RESOURCE GROUP rg1_cpu_test set cpu_max_percent 10;
ALTER RESOURCE GROUP rg2_cpu_test set cpu_max_percent 20;

-- prepare parallel queries in the two groups
10: SET ROLE TO role1_cpu_test;
11: SET ROLE TO role1_cpu_test;
12: SET ROLE TO role1_cpu_test;
13: SET ROLE TO role1_cpu_test;
14: SET ROLE TO role1_cpu_test;

20: SET ROLE TO role2_cpu_test;
21: SET ROLE TO role2_cpu_test;
22: SET ROLE TO role2_cpu_test;
23: SET ROLE TO role2_cpu_test;
24: SET ROLE TO role2_cpu_test;

--
-- now we get prepared.
--
-- on empty load the cpu usage shall be 0%
--
--
-- a group should not burst to use all the cpu usage
-- when it's the only one with running queries.
--
-- so the cpu usage shall be 0.9 * 10%
--

10&: SELECT * FROM busy;
11&: SELECT * FROM busy;
12&: SELECT * FROM busy;
13&: SELECT * FROM busy;
14&: SELECT * FROM busy;

-- start_ignore
1:TRUNCATE TABLE cpu_usage_samples;
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:TRUNCATE TABLE cpu_usage_samples;
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
-- end_ignore

-- verify it
1:SELECT verify_cpu_usage('rg1_cpu_test', 9, 2);

-- start_ignore
1:SELECT * FROM cancel_all;

10<:
11<:
12<:
13<:
14<:
-- end_ignore

10q:
11q:
12q:
13q:
14q:

10: SET ROLE TO role1_cpu_test;
11: SET ROLE TO role1_cpu_test;
12: SET ROLE TO role1_cpu_test;
13: SET ROLE TO role1_cpu_test;
14: SET ROLE TO role1_cpu_test;

--
-- when there are multiple groups with parallel queries,
-- they should follow the enforcement of the cpu usage.
--
-- rg1_cpu_test:rg2_cpu_test is 10:20, so:
--
-- - rg1_cpu_test gets 0.9 * 10%;
-- - rg2_cpu_test gets 0.9 * 20%;
--

10&: SELECT * FROM busy;
11&: SELECT * FROM busy;
12&: SELECT * FROM busy;
13&: SELECT * FROM busy;
14&: SELECT * FROM busy;

20&: SELECT * FROM busy;
21&: SELECT * FROM busy;
22&: SELECT * FROM busy;
23&: SELECT * FROM busy;
24&: SELECT * FROM busy;

-- start_ignore
1:TRUNCATE TABLE cpu_usage_samples;
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:TRUNCATE TABLE cpu_usage_samples;
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
1:SELECT fetch_sample();
1:SELECT pg_sleep(1.7);
-- end_ignore

1:SELECT verify_cpu_usage('rg1_cpu_test', 9, 2);
1:SELECT verify_cpu_usage('rg2_cpu_test', 18, 2);

-- start_ignore
1:SELECT * FROM cancel_all;

10<:
11<:
12<:
13<:
14<:

20<:
21<:
22<:
23<:
24<:

10q:
11q:
12q:
13q:
14q:


20q:
21q:
22q:
23q:
24q:

1q:
-- end_ignore

-- restore admin_group's cpu_max_percent
2:ALTER RESOURCE GROUP admin_group SET cpu_max_percent 10;

-- cleanup
2:REVOKE ALL ON FUNCTION complex_compute(int) FROM role1_cpu_test;
2:REVOKE ALL ON FUNCTION complex_compute(int) FROM role2_cpu_test;
2:REVOKE ALL ON busy FROM role1_cpu_test;
2:REVOKE ALL ON busy FROM role2_cpu_test;
2:DROP ROLE role1_cpu_test;
2:DROP ROLE role2_cpu_test;
2:DROP RESOURCE GROUP rg1_cpu_test;
2:DROP RESOURCE GROUP rg2_cpu_test;
