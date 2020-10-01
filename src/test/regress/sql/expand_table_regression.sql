-- start_ignore
create extension if not exists gp_debug_numsegments;
create language plpython3u;
-- end_ignore

drop schema if exists test_expand_table_regression cascade;
create schema test_expand_table_regression;
set search_path=test_expand_table_regression,public;

--
-- derived from src/pl/plpython/sql/plpython_trigger.sql
--
-- with some hacks we could insert data into incorrect segments, data expansion
-- should tolerant this.
--

-- with this trigger the inserted data is always hacked to '345'
create function trig345() returns trigger language plpython3u as $$
    TD["new"]["data"] = '345'
    return 'modify'
$$;

select gp_debug_set_create_table_default_numsegments(2);
create table b(data int);
select gp_debug_reset_create_table_default_numsegments();

-- by default '345' should be inserted on seg0
insert into b values ('345');
select gp_segment_id, * from b;

truncate b;

create trigger b_t before insert on b for each row execute procedure trig345();

-- however with the trigger it is inserted on seg1
insert into b select i from generate_series(1, 10) i;
select gp_segment_id, * from b;

-- data expansion should tolerant it
alter table b expand table;
select gp_segment_id, * from b;

-- start_ignore
-- We need to do a cluster expansion which will check if there are partial
-- tables, we need to drop the partial tables to keep the cluster expansion
-- run correctly.
reset search_path;
drop schema test_expand_table_regression cascade;
-- end_ignore
