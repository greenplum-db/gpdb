-- setup
5: drop table if exists mpp18562;
5: drop table if exists mpp18562_sessionid;
5: create table mpp18562(a int, b int) distributed by (a);
5: insert into mpp18562 select 1, 1;
-- save session id
5: CREATE TABLE mpp18562_sessionid(a, setting) AS SELECT 1, setting::int FROM pg_settings WHERE name = 'gp_session_id' distributed by (a);
2U: BEGIN;
2U: LOCK mpp18562 IN ACCESS EXCLUSIVE MODE;
5&: SELECT t1.* FROM mpp18562 t1 INNER JOIN mpp18562 t2 ON t1.b = t2.b;
-- all processes in the session 5 should be blocked
2U: SELECT count(case when not waiting then 1 end), count(*) > 0 FROM pg_stat_activity where sess_id = (SELECT setting FROM mpp18562_sessionid);
2U: COMMIT;
5<:
