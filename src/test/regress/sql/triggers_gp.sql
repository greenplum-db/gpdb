--
-- Extra GPDB tests for triggers
--
-- We generally claim that triggers are not supported in Greenplum, period.
-- But row-level AFTER triggers actually do work to some extent, even though
-- we've never documented how exactly they behave. This file covers those
-- cases.
--
-- The upstream tests in 'triggers' have pretty good coverage for these, too,
-- but it also tests a lot of cases that just error out in GPDB, so that it's
-- hard to follow which tests are behaving reasonably and which ones are not.
-- This file aims to cover the things that behave sanely, even though we don't
-- officially support anything to do with triggers.
--

create or replace function insert_notice_trig() returns trigger as $$
  begin
    raise notice 'insert trigger fired on % for %', TG_TABLE_NAME, TG_OP;
    return new;
  end;
$$ language plpgsql;

create or replace function update_notice_trig() returns trigger as $$
  begin
    raise notice 'update trigger fired on % for %', TG_TABLE_NAME, TG_OP;
    return new;
  end;
$$ language plpgsql;

create or replace function delete_notice_trig() returns trigger as $$
  begin
    raise notice 'delete trigger fired on % for %', TG_TABLE_NAME, TG_OP;
    return new;
  end;
$$ language plpgsql;

--
-- Simple non-partitioned case.
--

create table trigtest (nonkey int, distkey int)
  distributed by (distkey);
create trigger trig_ins_after after insert on trigtest
  for each row execute procedure insert_notice_trig();
create trigger trig_upd_after after update on trigtest
  for each row execute procedure update_notice_trig();
create trigger trig_del_after after delete on trigtest
  for each row execute procedure delete_notice_trig();

-- Inserts. Should fire the INSERT trigger.
insert into trigtest values (1, 1);
insert into trigtest values (2, 2);

-- Update non-key column. Should fire the UPDATE trigger.
update trigtest set nonkey = 3 where nonkey = 1;

-- Update distribution key column. Throws an error, currently.
update trigtest set distkey = 3 where distkey = 1;

-- Should fire the DELETE trigger.
delete from trigtest where nonkey = 2;

--
-- Triggers on a partitioned table
--

create table parted_trig (partkey int, nonkey int, distkey int)
  partition by list (partkey) distributed by (distkey);
create table parted_trig1 partition of parted_trig for values in (1);
create table parted_trig2 partition of parted_trig for values in (2);
create table parted_trig3 partition of parted_trig for values in (3);
create table parted_trig4 partition of parted_trig for values in (4);

/* Could create similar structure with this legacy GPDB syntax:
create table parted_trig (partkey int, nonkey int, distkey int)
  distributed by (distkey)
  partition by range (partkey) (start (1) end (5) every (1));
*/

create trigger trig_ins_after after insert on parted_trig
  for each row execute procedure insert_notice_trig();
create trigger trig_upd_after after update on parted_trig
  for each row execute procedure update_notice_trig();
create trigger trig_del_after after delete on parted_trig
  for each row execute procedure delete_notice_trig();

-- Inserts. Should fire the INSERT trigger.
insert into parted_trig values (1, 1, 1);
insert into parted_trig values (2, 2, 2);

-- Update non-key column. Should fire the UPDATE trigger.
update parted_trig set nonkey = 3 where nonkey = 1;

-- Update distribution key column. Throws an error, currently.
update parted_trig set distkey = 3 where distkey = 1;

-- Update partitioning key column. Should fire the DELETE+INSERT triggers,
-- like in PostgreSQL.
update parted_trig set partkey = 3 where partkey = 1;

-- Update everything in one statement. Throws an error, currently, because
-- updating the distributon key is not allowed.
update parted_trig set partkey = partkey + 1, distkey = distkey + 1;

-- Should fire the DELETE trigger.
delete from parted_trig where nonkey = 2;
