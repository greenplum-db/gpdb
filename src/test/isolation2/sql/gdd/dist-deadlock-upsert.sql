-- See github issue: https://github.com/greenplum-db/gpdb/issues/9449
-- insert into t values (x, x) on conflict (a, b) do update set b = yyy.
-- this kind of statement may lock tuples in segment and may lead to
-- global deadlock when GDD is enabled.

DROP TABLE IF EXISTS t_upsert;
CREATE TABLE t_upsert (id int, val int) distributed by (id);
CREATE UNIQUE INDEX uidx_t_upsert on t_upsert(id, val);
INSERT INTO t_upsert (id, val) SELECT i, i FROM generate_series(1, 100) i;

-- gang creation order is important, reset any guc to force the creation
10: RESET optimizer;
20: RESET optimizer;

10: BEGIN;
20: BEGIN;

10: INSERT INTO t_upsert VALUES (segid(0,1), segid(0,1)) on conflict (id, val) do update set val = 999;

20: INSERT INTO t_upsert VALUES (segid(1,1), segid(1,1)) on conflict (id, val) do update set val = 888;

select gp_inject_fault('gdd_probe', 'suspend', dbid)
  from gp_segment_configuration where content=-1 and role='p';
select gp_wait_until_triggered_fault('gdd_probe', 1, dbid)
  from gp_segment_configuration where content=-1 and role='p';
-- seg 0: con20 ==> con10, xid lock
20&: INSERT INTO t_upsert VALUES (segid(0,1), segid(0,1)) on conflict (id, val) do update set val = 666;

-- seg 1: con10 ~~> con20, tuple lock
10&: INSERT INTO t_upsert VALUES (segid(1,1), segid(1,1)) on conflict (id, val) do update set val = 777;

select gp_inject_fault('gdd_probe', 'reset', dbid)
  from gp_segment_configuration where content=-1 and role='p';
-- con20 will be cancelled by gdd
20<:
20q:

-- no more deadlock
10<:
10q:
