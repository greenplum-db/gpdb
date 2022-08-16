-- 
-- Test queries that can lead to deadlock. This is in a different file to
-- isolate the test & quickly identify it if ICG doesn't complete for a very
-- long time.
--


create schema deadlock;
set search_path = deadlock;

-- Test "Motion deadlock hazard" when Joins have Motions in both branches.
-- see src/backend/cdb/cdbllize.c

create table l (i int, j int) distributed by (i);
create table r (i int, j int) distributed by (j);

-- Sanity check the distribution hash algorithm to make sure value (1) and
-- value (2) are stored in different segments.
create table sanity_check_distribution (j int) distributed by (j);
insert into sanity_check_distribution values(1),(2); -- Values of l.j + 1 and r.j.
select count(distinct gp_segment_id) from sanity_check_distribution; -- should be 2.

insert into l select i, 1 from generate_series(1, 100000) i;  -- one segment destination
insert into r select i, 1 from generate_series(1, 100000) i;  -- one segment destination

set enable_mergejoin = off;
set enable_hashjoin = on;
set enable_nestloop = off;

-- Pick HJ
select count(*) from gp_dist_random('l') left join gp_dist_random('r') on l.j + 1 = r.j;


set enable_mergejoin = off;
set enable_hashjoin = off;
set enable_nestloop = on;

-- Pick NLJ, gp_dist_random() forces MJ
select count(*) from l left join r on l.j + 1 = r.j;

set enable_mergejoin = on;
set enable_hashjoin = off;
set enable_nestloop = off;

-- Pick MJ
select count(*) from gp_dist_random('l') left outer join gp_dist_random('r') on l.j + 1 = r.j;


drop schema if exists deadlock cascade;

-- Check gp_dist_wait_status not failing within a transaction
-- Github issue: https://github.com/greenplum-db/gpdb/issues/13795

BEGIN;
	select * from gp_dist_wait_status();
COMMIT;
