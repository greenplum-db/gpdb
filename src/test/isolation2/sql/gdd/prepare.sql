include: helpers/server_helpers.sql;

-- t0r is the reference table to provide the data distribution info.
DROP TABLE IF EXISTS t0p;
CREATE TABLE t0p (id int, val int);
INSERT INTO t0p (id, val) SELECT i, i FROM generate_series(1, 100) i;

DROP TABLE IF EXISTS t0r;
CREATE TABLE t0r (id int, val int, segid int) DISTRIBUTED REPLICATED;
INSERT INTO t0r (id, val, segid) SELECT id, val, gp_segment_id from t0p;

-- GDD tests rely on the data distribution, but depends on the number of
-- the segments the distribution might be different.
-- so we provide this helper function to return the nth id on a segment.
-- * `seg` is the segment id, starts from 0;
-- * `idx` is the index on the segment, starts from 1;
CREATE OR REPLACE FUNCTION segid(seg int, idx int)
RETURNS int AS $$
  SELECT id FROM t0r
  WHERE segid=$1
  ORDER BY id LIMIT 1 OFFSET ($2-1)
$$ LANGUAGE sql;

-- In some of the testcases the execution order of two background queries
-- must be enforced not only on master but also on segments, for example
-- in below case the order of 10 and 20 on segments results in different
-- waiting relations:
--
--     30: UPDATE t SET val=val WHERE id=1;
--     10&: UPDATE t SET val=val WHERE val=1;
--     20&: UPDATE t SET val=val WHERE val=1;
--
-- There is no perfect way to ensure this.  The '&' command in the isolation2
-- framework only ensures that the QD is being blocked, but this might not be
-- true on segments.  In fact on slow machines this exception occurs quite
-- offen on heave load. (e.g. when multiple testcases are executed in parallel)
--
-- So we provide this barrier function to ensure the execution order.
-- It's implemented with sleep now, but should at least work.
CREATE OR REPLACE FUNCTION barrier()
RETURNS void AS $$
  SELECT pg_sleep(4)
$$ LANGUAGE sql;

-- verify the function
-- Data distribution is sensitive to the underlying hash algorithm, we need each
-- segment has enough tuples for test, 10 should be enough.
SELECT segid(0,10) is not null;
SELECT segid(1,10) is not null;
SELECT segid(2,10) is not null;

-- table to just store the master's data directory path on segment.
CREATE TABLE datadir(a int, dir text);
INSERT INTO datadir select 1,datadir from gp_segment_configuration where role='p' and content=-1;

ALTER SYSTEM SET gp_enable_global_deadlock_detector TO on;
ALTER SYSTEM SET gp_global_deadlock_detector_period TO 5;

-- Use utility session on seg 0 to restart master. This way avoids the
-- situation where session issuing the restart doesn't disappear
-- itself.
1U:SELECT pg_ctl(dir, 'restart') from datadir;
-- Start new session on master to make sure it has fully completed
-- recovery and up and running again.
1: SHOW gp_enable_global_deadlock_detector;
1: SHOW gp_global_deadlock_detector_period;
