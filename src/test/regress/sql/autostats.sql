-- start_matchsubs
-- m/^LOG:  In mode on_change, command INSERT.* modifying 10 tuples caused Auto-ANALYZE./
-- s/\(dboid,tableoid\)=\(\d+,\d+\)/\(dboid,tableoid\)=\(XXXXX,XXXXX\)/
-- m/LOG:  Auto-stats did not issue ANALYZE on tableoid \d+ since the user does not have table-owner level permissions./
-- s/tableoid \d+/tableoid XXXXX/
-- end_matchsubs
-- start_matchignore
-- m/^LOG: .*Feature not supported: Non-default collation./
-- m/^LOG:.*ERROR,"PG exception raised"/
-- end_matchignore
set gp_autostats_mode=on_change;
set gp_autostats_on_change_threshold=9;
set log_autostats=on;
set client_min_messages=log;

drop table if exists autostats_test;
create table autostats_test (a INTEGER);
drop user if exists autostats_nonowner;
create user autostats_nonowner;

-- Make sure rel_tuples starts at zero
select relname, reltuples from pg_class where relname='autostats_test';

-- Try it with gp_autostats_allow_nonowner GUC enabled, but as a non-owner
-- without INSERT permission.  Should fail with permission denied, without
-- triggering autostats collection
set gp_autostats_allow_nonowner=on;
set role=autostats_nonowner;
insert into autostats_test select generate_series(1, 10);
select relname, reltuples from pg_class where relname='autostats_test';
reset role;

-- Try it with GUC enabled, after granting INSERT, stats should update to 10
grant insert on table autostats_test to autostats_nonowner;
set role=autostats_nonowner;
insert into autostats_test select generate_series(11, 20);
select relname, reltuples from pg_class where relname='autostats_test';

-- Try running analyze manually as nonowner, should fail
set role=autostats_nonowner;
analyze autostats_test;
select relname, reltuples from pg_class where relname='autostats_test';

-- Try to disable allow_nonowner GUC as ordinary user, should fail
set gp_autostats_allow_nonowner=off;
show gp_autostats_allow_nonowner;

-- GUC should still be enabled, stats should update to 20
insert into autostats_test select generate_series(21, 30);
select relname, reltuples from pg_class where relname='autostats_test';
reset role;

-- Change allow_nonowner GUC as admin, should work
set gp_autostats_allow_nonowner=off;
show gp_autostats_allow_nonowner;

-- GUC should be disabled, stats should not update
set role=autostats_nonowner;
insert into autostats_test select generate_series(31, 40);
select relname, reltuples from pg_class where relname='autostats_test';
reset role;

-- Try to enable allow_nonowner GUC as ordinary user, should fail

-- GUC should still be disabled, stats should update from 20 to 40
insert into autostats_test select generate_series(21, 30);
reset client_min_messages;
select relname, reltuples from pg_class where relname='autostats_test';
reset role;

-- After 4 successful inserts, final row count should also be 40
select COUNT(*) from autostats_test;

drop table if exists autostats_test;
drop user autostats_nonowner;

reset gp_autostats_mode;
reset gp_autostats_on_change_threshold;
reset log_autostats;
reset gp_autostats_allow_nonowner;
