-- start_ignore
DROP VIEW IF EXISTS cpu_status;
DROP VIEW IF EXISTS busy;
DROP VIEW IF EXISTS cancel_all;
DROP FUNCTION IF EXISTS round_percentage(text);
DROP TABLE IF EXISTS bigtable;
DROP ROLE IF EXISTS r1;
DROP ROLE IF EXISTS r2;
DROP RESOURCE GROUP g1;
DROP RESOURCE GROUP g2;
-- end_ignore

--
-- helper functions, tables and views
--

CREATE TABLE bigtable AS
	SELECT i AS c1, 'abc' AS c2
	FROM generate_series(1,100000) i;

-- the cpu usage limitation has an error rate about +-7.5%,
-- and also we want to satisfy the 0.1:0.2 rate under 90% overall limitation
-- so we round the cpu rate by 15%
CREATE FUNCTION round_percentage(text) RETURNS text AS $$
    SELECT (round(rtrim($1, '%') :: double precision / 15) * 15) :: text || '%'
$$ LANGUAGE sql;

CREATE VIEW cpu_status AS
    SELECT g.rsgname, round_percentage(s.cpu_usage)
    FROM gp_toolkit.gp_resgroup_status s, pg_resgroup g
    WHERE s.groupid=g.oid
    ORDER BY g.oid;

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
	SELECT pg_cancel_backend(procpid)
	FROM pg_stat_activity
	WHERE current_query LIKE 'SELECT * FROM busy%';

--
-- check gpdb cgroup configuration
--
-- cfs_quota_us := cfs_period_us * ncores * gp_resource_group_cpu_limit
-- shares := 1024 * ncores
--

! python -c "print $(cat /sys/fs/cgroup/cpu/gpdb/cpu.cfs_quota_us) == int($(cat /sys/fs/cgroup/cpu/gpdb/cpu.cfs_period_us) * $(nproc) * $(psql -d isolation2resgrouptest -Aqtc "SHOW gp_resource_group_cpu_limit"))";

! python -c "print $(cat /sys/fs/cgroup/cpu/gpdb/cpu.shares) == 1024 * 256";

--
-- check default groups configuration
--
-- SUB/shares := TOP/shares * cpu_rate_limit
--

! python -c "print $(cat /sys/fs/cgroup/cpu/gpdb/$(psql -d isolation2resgrouptest -Aqtc "SELECT oid FROM pg_resgroup WHERE rsgname='default_group'")/cpu.shares) == int($(cat /sys/fs/cgroup/cpu/gpdb/cpu.shares) * $(psql -d isolation2resgrouptest -Aqtc "SELECT value FROM pg_resgroupcapability c, pg_resgroup g WHERE c.resgroupid=g.oid AND reslimittype=2 AND g.rsgname='default_group'"))";

! python -c "print $(cat /sys/fs/cgroup/cpu/gpdb/$(psql -d isolation2resgrouptest -Aqtc "SELECT oid FROM pg_resgroup WHERE rsgname='admin_group'")/cpu.shares) == int($(cat /sys/fs/cgroup/cpu/gpdb/cpu.shares) * $(psql -d isolation2resgrouptest -Aqtc "SELECT value FROM pg_resgroupcapability c, pg_resgroup g WHERE c.resgroupid=g.oid AND reslimittype=2 AND g.rsgname='admin_group'"))";

-- create two resource groups
CREATE RESOURCE GROUP g1 WITH (concurrency=5, cpu_rate_limit=0.1, memory_limit=0.2);
CREATE RESOURCE GROUP g2 WITH (concurrency=5, cpu_rate_limit=0.2, memory_limit=0.2);

-- check g1 configuration
! python -c "print $(cat /sys/fs/cgroup/cpu/gpdb/$(psql -d isolation2resgrouptest -Aqtc "SELECT oid FROM pg_resgroup WHERE rsgname='g1'")/cpu.shares) == int($(cat /sys/fs/cgroup/cpu/gpdb/cpu.shares) * 0.1)";

-- check g2 configuration
! python -c "print $(cat /sys/fs/cgroup/cpu/gpdb/$(psql -d isolation2resgrouptest -Aqtc "SELECT oid FROM pg_resgroup WHERE rsgname='g2'")/cpu.shares) == int($(cat /sys/fs/cgroup/cpu/gpdb/cpu.shares) * 0.2)";

-- create two roles and assign them to above groups
CREATE ROLE r1 RESOURCE GROUP g1;
CREATE ROLE r2 RESOURCE GROUP g2;
GRANT ALL ON busy TO r1;
GRANT ALL ON busy TO r2;

-- prepare parallel queries in the two groups
10: SET ROLE TO r1;
11: SET ROLE TO r1;
12: SET ROLE TO r1;
13: SET ROLE TO r1;
14: SET ROLE TO r1;

20: SET ROLE TO r2;
21: SET ROLE TO r2;
22: SET ROLE TO r2;
23: SET ROLE TO r2;
24: SET ROLE TO r2;

--
-- now we get prepared.
--
-- on empty load the cpu usage shall be 0%
--

SELECT * FROM cpu_status;

--
-- a group should burst to use all the cpu usage
-- when it's the only one with running queries.
--
-- however the overall cpu usage is controlled by a GUC
-- gp_resource_group_cpu_limit which is 90% by default.
--
-- so the cpu usage shall be 90%
--

10&: SELECT * FROM busy;
11&: SELECT * FROM busy;
12&: SELECT * FROM busy;
13&: SELECT * FROM busy;
14&: SELECT * FROM busy;

SELECT pg_sleep(20);
SELECT * FROM cpu_status;

-- start_ignore
SELECT * FROM cancel_all;

10<:
11<:
12<:
13<:
14<:
-- end_ignore

--
-- when there are multiple groups with parallel queries,
-- they should share the cpu usage by their cpu_usage settings,
--
-- g1:g2 is 0.1:0.2 => 1:2, so:
--
-- - g1 gets 90% * 1/3 => 30%;
-- - g2 gets 90% * 2/3 => 60%;
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

SELECT pg_sleep(20);
SELECT * FROM cpu_status;

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
-- end_ignore
