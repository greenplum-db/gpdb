-- This test validates, reader and writer both wait if lock is not
-- available. There used to be a bug where reader didn't wait even if lock was
-- held by some other session.

-- setup
1: create table reader_waits_for_lock_table(a int, b int) distributed by (a);
1: insert into reader_waits_for_lock_table select 1, 1;
-- save session id
1: CREATE TABLE reader_waits_for_lock_table_sessionid(a, setting) AS SELECT 1, setting::int FROM pg_settings WHERE name = 'gp_session_id' distributed by (a);
0U: BEGIN;
0U: LOCK reader_waits_for_lock_table IN ACCESS EXCLUSIVE MODE;
-- creates reader and writer gang
1&: SELECT t1.* FROM reader_waits_for_lock_table t1 INNER JOIN reader_waits_for_lock_table t2 ON t1.b = t2.b;
-- all processes in the session 1 should be blocked
0U: SELECT count(case when waiting then 1 end) = count(*) all_waiting FROM pg_stat_activity where sess_id = (SELECT setting FROM reader_waits_for_lock_table_sessionid);
0U: COMMIT;
1<:
