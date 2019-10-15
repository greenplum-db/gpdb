-- OOM should be raised when there are too many idle processes.
--
-- We have below assumptions in the tests:
-- - number of primary segments: 3;
-- - the startup memory cost for a postgres process: 8MB ~ 14MB;
-- - per query limit: 20MB;
-- - per segment limit: 60MB;

-- start_matchsubs
--
-- m/DETAIL:  Per-query VM protect limit reached: current limit is \d+ kB, requested \d+ bytes, available \d+ MB/
-- s/\d+/XXX/g
--
-- m/DETAIL:  VM Protect failed to allocate \d+ bytes, \d+ MB available/
-- s/\d+/XXX/g
--
-- m/(seg\d+ \d+.\d+.\d+.\d+:\d+)/
-- s/(.*)/(seg<ID> IP:PORT)/
--
-- end_matchsubs

--
-- To reach the per query limit we need at least 3 slices in one query.
--

1: select *
     from gp_dist_random('gp_id') t1
     join gp_dist_random('gp_id') t2 using(gpname)
     join gp_dist_random('gp_id') t3 using(gpname)
     join gp_dist_random('gp_id') t4 using(gpname)
   ;
1q:

--
-- To reach the per segment limit we need at least 8 concurrent sessions.
--

-- However the per segment limit is not enforced on QD, so below QD-only
-- sessions could all run successfully.
1: select 1;
2: select 1;
3: select 1;
4: select 1;
5: select 1;
6: select 1;
7: select 1;
8: select 1;

-- The per segment limit should be reached on one or more QEs in below
-- sessions, we only care about the last one.
-- start_ignore
1: set gp_vmem_idle_resource_timeout to '1h';
2: set gp_vmem_idle_resource_timeout to '1h';
3: set gp_vmem_idle_resource_timeout to '1h';
4: set gp_vmem_idle_resource_timeout to '1h';
5: set gp_vmem_idle_resource_timeout to '1h';
6: set gp_vmem_idle_resource_timeout to '1h';
7: set gp_vmem_idle_resource_timeout to '1h';
-- end_ignore
8: set gp_vmem_idle_resource_timeout to '1h';
