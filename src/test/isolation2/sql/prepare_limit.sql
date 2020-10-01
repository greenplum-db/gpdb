-- test to verify a bug that causes standby startup fatal with message like
-- "the limit of xxx distributed transactions has been reached".
-- Refer comment in https://github.com/greenplum-db/gpdb/issues/9207 for the
-- context.
include: helpers/server_helpers.sql;

-- We will reset the value to 250 finally so sanity check the current value here.
6: show max_prepared_transactions;
!\retcode gpconfig -c max_prepared_transactions -v 3 --skipvalidation;
!\retcode gpstop -ari;

5: create table prepare_limit1 (a int);
5: create table prepare_limit2 (a int);
5: create table prepare_limit3 (a int);
5: create table prepare_limit4 (a int);

5: select gp_inject_fault_infinite('dtm_before_insert_forget_comitted', 'suspend', 1);

-- Note first insert after table create triggers auto_stats and leads to 2pc
-- transaction.

-- (2) is on seg0
1&: insert into prepare_limit1 values(2);
2&: insert into prepare_limit2 values(2);

-- (1) is on seg1
3&: insert into prepare_limit3 values(1);
4&: insert into prepare_limit4 values(1);

-- wait until these 2pc reach before inserting forget commit.
5: SELECT gp_wait_until_triggered_fault('dtm_before_insert_forget_comitted', 4, 1);

-- wait until standby catches up and replays all xlogs.
5: select wait_for_replication_replay (-1, 5000);

-- reset to make testing continue
5: select gp_inject_fault('dtm_before_insert_forget_comitted', 'reset', 1);
1<:
2<:
3<:
4<:

-- verify that standby is correctly wal streaming.
5: select state from pg_stat_replication;

-- verify the tuples are on correct segments so the test assumption is
-- correct. (i.e. tuple 2, 1 are on different segments).
5: select gp_segment_id, * from prepare_limit1;
5: select gp_segment_id, * from prepare_limit2;
5: select gp_segment_id, * from prepare_limit3;
5: select gp_segment_id, * from prepare_limit4;

-- cleanup
5: drop table prepare_limit1;
5: drop table prepare_limit2;
5: drop table prepare_limit3;
5: drop table prepare_limit4;

-- Not using gpconfig -r, else it makes max_prepared_transactions be default
-- (50) and some isolation2 tests will fail due to "too many clients". Hardcode
-- to 250 which is the default value when demo cluster is created.
!\retcode gpconfig -c max_prepared_transactions -v 250 --skipvalidation;
!\retcode gpstop -ari;
