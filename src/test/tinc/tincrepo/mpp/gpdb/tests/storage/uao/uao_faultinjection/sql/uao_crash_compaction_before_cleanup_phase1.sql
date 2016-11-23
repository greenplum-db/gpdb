-- start_ignore
SET gp_create_table_random_default_distribution=off;

-- Due to the fault injected by our code, VACUUM will cause the segment
-- to panic and go into recovery. The QD may need more than the default 5
-- retries in order to run the INSERT on the segment after it's done
-- recovering, thus we set this GUC to 8. This situation can happen if
-- the XLog on the segment is larger than usual due to previous test
-- suites running, which causes XLog recovery to take longer.
SET gp_gang_creation_retry_count=8;
-- end_ignore
VACUUM foo;
INSERT INTO foo VALUES(1, 1, 'c');
SELECT a, b FROM foo order by a;
