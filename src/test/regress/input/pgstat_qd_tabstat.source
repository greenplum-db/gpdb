create extension if not exists gp_debug_numsegments;
-- Turn off auto_stat to avoid ANALYZE
set gp_autostats_mode = none;

-- Test pgstat table stat for copy on QD
create table table_for_docopy(i int, j varchar) distributed by (i);
copy table_for_docopy (i, j) from stdin;
2	hello2
1	hello1
3	hello3
\.
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_docopy'::regclass;

CREATE TABLE data_tbl (a int,b char) distributed by (a);
INSERT INTO data_tbl values(1,'1');
INSERT INTO data_tbl values(1,'2');
INSERT INTO data_tbl values(1,'a');
INSERT INTO data_tbl values(1,'b');
COPY data_tbl TO '/tmp/data_tbl<SEGID>.csv' on segment;
create table copy_on_segment (a int,b char);
COPY copy_on_segment from '/tmp/data_tbl<SEGID>.csv' on segment log errors segment reject limit 3 rows;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'copy_on_segment'::regclass;


-- Test pgstat table stat in initplan on QD
create table table_for_initplan (i int, j varchar, k int) distributed by (i);
copy table_for_initplan (i, j, k) from stdin;
1	test1	1
2	test2	2
3	test3	3
\.
explain (costs off) with updated AS (update table_for_initplan set k = 33 where i = 3 returning k) select table_for_initplan.*, (select sum(k) from updated) from table_for_initplan;
with updated AS (update table_for_initplan set k = 33 where i = 3 returning k) select table_for_initplan.*, (select sum(k) from updated) from table_for_initplan;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_initplan'::regclass;


-- Test pgstat table stat in CTAS on QD
create table table_for_ctas with (autovacuum_enabled=false) as select i, 'hello' || i from generate_series(1, 100) f(i);
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_ctas'::regclass;
select i, 'hello' || i into table_for_insert_into from generate_series(1, 100) f(i);
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_insert_into'::regclass;


-- Test pgstat table stat in ALTER TABLE SET DISTRIBUTED BY on QD
create table table_for_set_distributed_by(i int, j varchar) distributed by (i);
insert into table_for_set_distributed_by select i, 'hello' || i from generate_series(1, 333) f(i);
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_set_distributed_by'::regclass;
alter table table_for_set_distributed_by set distributed by (j);
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_set_distributed_by'::regclass;


-- Test pgstat table stat in execution of funciton on QD
create table table_for_function(i int, j varchar) distributed by (i);
create function update_table_for_function() returns void as
$$
  begin
    insert into table_for_function select i, 'hello' || i from generate_series(1, 333) f(i);
    delete from table_for_function where i <= 200;
  end
$$
language plpgsql volatile;
select update_table_for_function();
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_function'::regclass;


-- Test pgstat table stat in ALTER TABLE EXPAND TABLE on QD;
-- Thanks @uglthinx for telling me this way to execute ALTER TABLE EXPAND manually
select gp_debug_set_create_table_default_numsegments(1);
create table table_for_expand(i int, j varchar) distributed by (i);
insert into table_for_expand select i, 'hello' || i from generate_series(1, 333) f(i);
select count(distinct gp_segment_id) from table_for_expand;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_expand'::regclass;
alter table table_for_expand expand table;
select count(distinct gp_segment_id) from table_for_expand;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_expand'::regclass;
select gp_debug_reset_create_table_default_numsegments();


-- Test pgstat table stat in INSERT/UPDATE/DELETE on QD
create table table_for_iud(i int, j varchar) distributed by (i);
begin;
insert into table_for_iud select i, 'hello' || i from generate_series(1, 333) f(i);
savepoint level1;
savepoint level2;
savepoint level3;
savepoint level4;
savepoint level5;
savepoint level6;
savepoint level7;
delete from table_for_iud where i <= 200;
rollback to savepoint level5;
update table_for_iud set j = 'heroes never die' where i >= 300;
release savepoint level3;
commit;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_live_tup, n_dead_tup from pg_stat_all_tables_internal where relid = 'table_for_iud'::regclass;

begin;
savepoint level1;
savepoint level2;
savepoint level3;
savepoint level4;
savepoint level5;
savepoint level6;
savepoint level7;
insert into table_for_iud select i, 'hello' || i from generate_series(333, 777) f(i);
rollback to savepoint level5;
update table_for_iud set j = 'are you ok' where i >= 300;
rollback to savepoint level3;
delete from table_for_iud where i <= 200;
commit;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_live_tup, n_dead_tup from pg_stat_all_tables_internal where relid = 'table_for_iud'::regclass;


-- Test pgstat table stat in TRUNCATE on QD
create table table_for_truncate(i int, j varchar) distributed by (i);
insert into table_for_truncate select i, 'hello' || i from generate_series(1, 777) f(i);
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'table_for_truncate'::regclass;
begin;
savepoint level1;
savepoint level2;
savepoint level3;
update table_for_truncate set j = 'D' where i <= 200;
delete from table_for_truncate where i >= 700;
insert into table_for_truncate select i, 'hello' || i from generate_series(778, 800) f(i);
truncate table_for_truncate;
insert into table_for_truncate select i, 'hello' || i from generate_series(1, 800) f(i);
delete from table_for_truncate where i >= 700;
update table_for_truncate set j = 'D' where i <= 200;
commit;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_live_tup, n_dead_tup from pg_stat_all_tables_internal where relid = 'table_for_truncate'::regclass;

create table table_for_truncate_abort(i int, j varchar) distributed by (i);
insert into table_for_truncate_abort select i, 'hello' || i from generate_series(1, 777) f(i);
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_live_tup, n_dead_tup from pg_stat_all_tables_internal where relid = 'table_for_truncate_abort'::regclass;
begin;
savepoint level1;
savepoint level2;
savepoint level3;
update table_for_truncate_abort set j = 'D' where i <= 200;
delete from table_for_truncate_abort where i >= 700;
insert into table_for_truncate_abort select i, 'hello' || i from generate_series(778, 800) f(i);
truncate table_for_truncate_abort;
insert into table_for_truncate_abort select i, 'hello' || i from generate_series(1, 800) f(i);
delete from table_for_truncate_abort where i < 700;
update table_for_truncate_abort set j = 'D' where i >= 200;
rollback;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_live_tup, n_dead_tup from pg_stat_all_tables_internal where relid = 'table_for_truncate_abort'::regclass;


-- Test pgstat table stat for partition table on QD
CREATE TABLE rankpart (id int, rank int, product int)
DISTRIBUTED BY (id)
PARTITION BY RANGE (rank)
( START (1) END (10) EVERY (5),
  DEFAULT PARTITION extra );
insert into rankpart select i, i % 10, i from generate_series(1, 1000)i;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_2'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_3'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_extra'::regclass;

begin;
delete from rankpart where id <= 100;
rollback;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_2'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_3'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_extra'::regclass;

copy rankpart (id, rank, product) from stdin;
1001	1	1001
1002	1	1002
1003	1	1003
1004	1	1004
1005	1	1005
1006	6	1006
1007	6	1007
1008	6	1008
1009	6	1009
1010	6	1010
\.
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_2'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_3'::regclass;

begin;
update rankpart set rank = 1 where id > 1005;
savepoint level1;
update rankpart set rank = 6 where id > 1005;
savepoint level2;
delete from rankpart where id > 1005;
release savepoint level2;
rollback to savepoint level1;
commit;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_2'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_3'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_extra'::regclass;

begin;
savepoint level1_1;
insert into rankpart select i, i % 10, i from generate_series(2001, 3000)i;
insert into rankpart select i, i % 10, i from generate_series(3001, 4000)i;
commit;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_2'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_3'::regclass;
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'rankpart_1_prt_extra'::regclass;


-- Test pgstat matview stat with distributed policy.
create table base_table(i int, j int, z int ) distributed by (i);
insert into base_table select i,i,i from generate_series(1, 100) i;
create materialized view mt as select * from base_table where z>=50;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'mt'::regclass;
insert into base_table select i,i,i from generate_series(1, 100) i;
refresh materialized view mt;
select pg_sleep(0.77) from gp_dist_random('gp_id'); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'mt'::regclass;
-- pg_stat_all_tables collects gpstats across segments
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables where relid = 'mt'::regclass;

drop materialized view mt;
drop table base_table;

-- Test pgstat matview stat with replicated policy.
create table base_table(i int, j int, z int ) distributed replicated;
insert into base_table select i,i,i from generate_series(1, 100) i;
create materialized view mt as select * from base_table where z>=50 distributed replicated;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'mt'::regclass;
insert into base_table select i,i,i from generate_series(1, 100) i;
refresh materialized view mt;
select pg_sleep(0.77) from gp_dist_random('gp_id'); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables_internal where relid = 'mt'::regclass;
-- pg_stat_all_tables collects gpstats across segments
select n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, n_live_tup, n_dead_tup, n_mod_since_analyze from pg_stat_all_tables where relid = 'mt'::regclass;

reset gp_autostats_mode;


-- Test stats creation for AO auxiliary tables
create table tabstat_ao(i int, j int) using ao_row distributed by (i);
create index on tabstat_ao(i);
insert into tabstat_ao select 1,1;
delete from tabstat_ao;
select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.

select count(*) from pg_stat_all_tables
where
    relid = (select segrelid from pg_appendonly where relid = 'tabstat_ao'::regclass)
    OR relid = (select blkdirrelid from pg_appendonly where relid = 'tabstat_ao'::regclass)
    OR relid = (select visimaprelid from pg_appendonly where relid = 'tabstat_ao'::regclass);

select pg_sleep(0.77); -- Force pgstat_report_stat() to send tabstat.
select n_tup_ins from pg_stat_all_tables where relid = (select segrelid from pg_appendonly where relid = 'tabstat_ao'::regclass);
select n_tup_ins from pg_stat_all_tables where relid = (select blkdirrelid from pg_appendonly where relid = 'tabstat_ao'::regclass);
select n_tup_ins from pg_stat_all_tables where relid = (select visimaprelid from pg_appendonly where relid = 'tabstat_ao'::regclass);

drop table tabstat_ao;
