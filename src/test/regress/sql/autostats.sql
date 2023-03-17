-- start_matchsubs
-- m/^LOG:  In mode on_change, command INSERT.* modifying 10 tuples caused Auto-ANALYZE./
-- s/\(dboid,tableoid\)=\(\d+,\d+\)/\(dboid,tableoid\)=\(XXXXX,XXXXX\)/
-- m/LOG:  Auto-stats did not issue ANALYZE on tableoid \d+ since the user does not have table-owner level permissions./
-- s/tableoid \d+/tableoid XXXXX/
-- end_matchsubs
-- start_matchignore
-- m/^LOG: .*Feature not supported: Queries on master-only tables./
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

-- test inFunction
-- udf
create function test_auto_stats_in_function(sql text, load_data boolean, check_relname text) returns void as
$$
declare
  ntuples int;
begin
  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=none;
  set gp_autostats_mode_in_functions=none;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=none, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';

  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=on_no_stats;
  set gp_autostats_mode_in_functions=none;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=on_no_stats, gp_autostats_mode_in_functions=none, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';

  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=none;
  set gp_autostats_mode_in_functions=on_no_stats;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=on_no_stats, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';

  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=none;
  set gp_autostats_mode_in_functions=on_no_stats;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=on_no_stats, ntuples=%', ntuples;
  set gp_autostats_mode_in_functions=on_change;
  set gp_autostats_on_change_threshold=0;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=on_change, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';
end;
$$
language plpgsql;

select test_auto_stats_in_function('copy t_test_auto_stats_in_function from program ''echo 1''',
                                   false, 't_test_auto_stats_in_function');
select test_auto_stats_in_function('create table tmp_test_auto_stats_in_function as select * from t_test_auto_stats_in_function distributed randomly',
                                   true, 'tmp_test_auto_stats_in_function');
select test_auto_stats_in_function('delete from t_test_auto_stats_in_function',
                                   true, 't_test_auto_stats_in_function');

drop function test_auto_stats_in_function(text, boolean, text);
-- procedure
create procedure test_auto_stats_in_function(sql text, load_data boolean, check_relname text) as
$$
declare
  ntuples int;
begin
  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=none;
  set gp_autostats_mode_in_functions=none;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=none, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';

  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=on_no_stats;
  set gp_autostats_mode_in_functions=none;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=on_no_stats, gp_autostats_mode_in_functions=none, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';

  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=none;
  set gp_autostats_mode_in_functions=on_no_stats;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=on_no_stats, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';

  execute 'create table t_test_auto_stats_in_function(a int) distributed randomly';
  set gp_autostats_mode=none;
  set gp_autostats_mode_in_functions=on_no_stats;
  if load_data then execute 'insert into t_test_auto_stats_in_function values (1)'; end if;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=on_no_stats, ntuples=%', ntuples;
  set gp_autostats_mode_in_functions=on_change;
  set gp_autostats_on_change_threshold=0;
  execute sql;
  select reltuples into ntuples from pg_class where relname = check_relname;
  raise info 'gp_autostats_mode=none, gp_autostats_mode_in_functions=on_change, ntuples=%', ntuples;
  execute 'drop table t_test_auto_stats_in_function';
  execute 'drop table if exists tmp_test_auto_stats_in_function';
end;
$$
language plpgsql;

call test_auto_stats_in_function('copy t_test_auto_stats_in_function from program ''echo 1''',
                                   false, 't_test_auto_stats_in_function');
call test_auto_stats_in_function('create table tmp_test_auto_stats_in_function as select * from t_test_auto_stats_in_function distributed randomly',
                                   true, 'tmp_test_auto_stats_in_function');
call test_auto_stats_in_function('delete from t_test_auto_stats_in_function',
                                   true, 't_test_auto_stats_in_function');

drop procedure test_auto_stats_in_function(text, boolean, text);

create table t_test_auto_stats_in_function(a int);
set gp_autostats_mode_in_functions = none;
set gp_autostats_mode = on_no_stats;
copy t_test_auto_stats_in_function from program 'echo 1';
select reltuples from pg_class where relname = 't_test_auto_stats_in_function';
drop table t_test_auto_stats_in_function;

create table t_test_auto_stats_in_function(a int);
copy t_test_auto_stats_in_function from program 'echo 1';
set gp_autostats_mode_in_functions = none;
set gp_autostats_mode = on_no_stats;
create table tmp_test_auto_stats_in_function as select * from t_test_auto_stats_in_function distributed randomly;
select reltuples from pg_class where relname = 't_test_auto_stats_in_function';
drop table t_test_auto_stats_in_function;
drop table tmp_test_auto_stats_in_function;

-- reset GUCs

reset gp_autostats_mode;
reset gp_autostats_mode_in_functions;
reset gp_autostats_on_change_threshold;
reset log_autostats;
reset gp_autostats_allow_nonowner;
