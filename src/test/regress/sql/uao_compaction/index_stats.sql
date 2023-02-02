-- @Description Tests basic index stats after vacuuming

CREATE TABLE mytab(
          col_int int,
          col_text text,
          col_numeric numeric,
          col_unq int
          ) with(appendonly=true) DISTRIBUTED RANDOMLY;

Create index mytab_int_idx1 on mytab(col_int);

insert into mytab values(1,'aa',1001,101),(2,'bb',1002,102);

select * from mytab;
update mytab set col_text=' new value' where col_int = 1;
select * from mytab;
vacuum mytab;
SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab';
SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab_int_idx1';

-- Test to ensure that reltuples is updated for an index after lazy vacuum.
-- This is vital as most index AMs that depend on this tuple count (eg btree, bitmap etc)
-- which is passed up from the table AM during lazy vacuum.
-- create a fresh table for the test
CREATE TABLE mytab2(
          col_int int,
          col_text text,
          col_numeric numeric,
          col_unq int
          ) with(appendonly=true) DISTRIBUTED BY (col_int);

create index mytab2_int_idx1 on mytab2 using bitmap(col_int);

insert into mytab2 values(1,'aa',1001,101),(2,'bb',1002,102);

SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab2_int_idx1';

-- first vacuum collect table stat on segments
vacuum mytab2;
-- inspect the state of the stats on segments
SELECT gp_segment_id, relname, reltuples FROM gp_dist_random('pg_class') WHERE relname = 'mytab2_int_idx1';
-- second vacuum update index stat with table stat
vacuum mytab2;
-- inspect the state of the stats on segments
SELECT gp_segment_id, relname, reltuples FROM gp_dist_random('pg_class') WHERE relname = 'mytab2_int_idx1';
SELECT relname, reltuples FROM pg_class WHERE relname = 'mytab2_int_idx1';

-- Test correctness of index->reltuples in consecutively VACUUM.
CREATE TABLE mytab3(
          col_int int,
          col_text text,
          col_numeric numeric,
          col_unq int
          ) with(appendonly=true) DISTRIBUTED BY (col_int);

create index mytab3_int_idx1 on mytab3(col_int);

insert into mytab3 values(1,'aa',1001,101),(2,'bb',1002,102);

select reltuples from pg_class where relname='mytab3';
-- inspect the state of the stats on segments
select gp_segment_id, relname, reltuples from gp_dist_random('pg_class') where relname = 'mytab3_int_idx1';
select reltuples from pg_class where relname='mytab3_int_idx1';
-- 1st VACUUM, expect reltuples = 2
vacuum mytab3;
select reltuples from pg_class where relname='mytab3';
-- inspect the state of the stats on segments
select gp_segment_id, relname, reltuples from gp_dist_random('pg_class') where relname = 'mytab3_int_idx1';
select reltuples from pg_class where relname='mytab3_int_idx1';
-- 2nd VACUUM, expect reltuples = 2
vacuum mytab3;
select reltuples from pg_class where relname='mytab3';
-- inspect the state of the stats on segments
select gp_segment_id, relname, reltuples from gp_dist_random('pg_class') where relname = 'mytab3_int_idx1';
select reltuples from pg_class where relname='mytab3_int_idx1';

-- Prior to this fix, the case would be failed here. Given the
-- scenario of updating stats during VACUUM:
-- 1) coordinator vacuums and updates stats of its own;
-- 2) then coordinator dispatches vacuum to segments;
-- 3) coordinator combines stats received from segments to overwrite the stats of its own.
-- Because upstream introduced a feature which could skip full index scan uring cleanup
-- of B-tree indexes when possible (refer to:
-- https://github.com/postgres/postgres/commit/857f9c36cda520030381bd8c2af20adf0ce0e1d4),
-- there was a case in QD-QEs distributed deployment that some QEs could skip full index scan and
-- stop updating statistics, result in QD being unable to collect all QEs' stats thus overwrote
-- a paritial accumulated value to index->reltuples. More interesting, it usually happened starting
-- from the 3rd time of consecutively VACUUM after fresh inserts due to above skipping index scan
-- criteria.
-- 3rd VACUUM, expect reltuples = 2
vacuum mytab3;
select reltuples from pg_class where relname='mytab3';
-- inspect the state of the stats on segments
select gp_segment_id, relname, reltuples from gp_dist_random('pg_class') where relname = 'mytab3_int_idx1';
select reltuples from pg_class where relname='mytab3_int_idx1';

drop table mytab;
drop table mytab2;
drop table mytab3;
