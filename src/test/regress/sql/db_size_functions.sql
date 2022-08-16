--
-- Tests on the pg_database_size(), pg_tablespace_size(), pg_relation_size(), etc.
-- functions.
--
-- These functions exist in PostgreSQL, but they have been modified in GPDB,
-- to collect the totals across segments, and to support AO / AOCS tables.
-- Hence, we better have extra tests for those things.
--


-- The total depends on the number of segments, and will also change whenever
-- the built-in objects change, so be lenient.
-- As of this writing, the total size of template0 database, across three segments,
-- is 67307536 bytes.
select pg_database_size('template0'::name) between 40000000 and 200000000;
select pg_database_size(1::oid) = pg_database_size('template1'::name);

-- 19713632 bytes, as of this writing
select pg_tablespace_size('pg_global'::name) between 10000000 and 50000000;
select pg_tablespace_size(1664::oid) between 10000000 and 50000000;
select pg_tablespace_size('pg_global'::name) = pg_tablespace_size(1664::oid);

-- Non-existent name/OID. These should return NULL or throw an error,
-- depending on the variant.
select pg_database_size('nonexistent');
select pg_database_size(9999);
select pg_tablespace_size('nonexistent');
select pg_tablespace_size(9999);

select pg_relation_size(9999);
select pg_table_size(9999);
select pg_indexes_size(9999);
select pg_total_relation_size(9999);

-- Test on relations that have no storage (pg_tables is a view)
select pg_relation_size('pg_tables');
select pg_table_size('pg_tables');
select pg_indexes_size('pg_tables');
select pg_total_relation_size('pg_tables');

-- Test on functions are not allowed to run on entryDB.
create temp table t1 as select pg_relation_size('pg_tables') from pg_class limit 1;
create temp table t1 as select pg_total_relation_size('pg_tables') from pg_class limit 1;
create temp table t1 as select gp_segment_id as seg_id from gp_tablespace_location((SELECT oid FROM pg_tablespace WHERE spcname='pg_default'));
create temp table t1 as select gp_segment_id as seg_id from gp_switch_wal();

--
-- Tests on the table and index size variants.
--
CREATE TABLE heapsizetest (a int);

-- First test with an empty table and no indexes. Should be all zeros.
select pg_relation_size('heapsizetest');
select pg_table_size('heapsizetest');
select pg_indexes_size('heapsizetest');
select pg_total_relation_size('heapsizetest');

-- Now test with a non-empty table (still no indexes, though).
insert into heapsizetest select generate_series(1, 100000);
vacuum heapsizetest;

-- Check that the values are in an expected ranges.
select pg_relation_size('heapsizetest') between 3000000 and 5000000; -- 3637248
select pg_relation_size('heapsizetest', 'fsm') between 250000 and 350000; -- 294912
select pg_table_size('heapsizetest') between 3000000 and 5000000; -- 4030464
select pg_table_size('heapsizetest') > pg_relation_size('heapsizetest');
select pg_indexes_size('heapsizetest');
select pg_total_relation_size('heapsizetest') between 3000000 and 5000000; -- 4030464
select pg_total_relation_size('heapsizetest') = pg_table_size('heapsizetest');

-- Now also indexes
create index on heapsizetest(a);

select pg_relation_size('heapsizetest') between 3000000 and 5000000; -- 3637248
select pg_table_size('heapsizetest') between 3000000 and 5000000; -- 4030464
select pg_indexes_size('heapsizetest') between 2000000 and 3000000; -- 2490368
select pg_total_relation_size('heapsizetest') between 6000000 and 7000000; -- 6520832
select pg_total_relation_size('heapsizetest') = pg_table_size('heapsizetest') + pg_indexes_size('heapsizetest');

-- Test on AO and AOCS tables
CREATE TABLE aosizetest (a int) WITH (appendonly=true, orientation=row);
insert into aosizetest select generate_series(1, 100000);
select pg_relation_size('aosizetest') between 750000 and 1500000; -- 1001648
select pg_relation_size('aosizetest', 'fsm'); -- 0
select pg_table_size('aosizetest') between 1000000 and 1500000; -- 1263792
select pg_table_size('aosizetest') > pg_relation_size('aosizetest');
select pg_total_relation_size('aosizetest') between 1000000 and 1500000; -- 1263792
select pg_total_relation_size('aosizetest') = pg_table_size('aosizetest');

CREATE TABLE aocssizetest (a int, col1 int, col2 text) WITH (appendonly=true, orientation=column);
insert into aocssizetest select g, g, 'x' || g from generate_series(1, 100000) g;
select pg_relation_size('aocssizetest') between 1000000 and 2000000; -- 1491240
select pg_relation_size('aocssizetest', 'fsm'); -- 0
select pg_table_size('aocssizetest') between 1500000 and 3000000; -- 1884456
select pg_table_size('aocssizetest') > pg_relation_size('aocssizetest');
select pg_total_relation_size('aocssizetest') between 1500000 and 3000000; -- 1884456
select pg_total_relation_size('aocssizetest') = pg_table_size('aocssizetest');

-- Test when the auxiliary relation of AO table is corrupted, the database will not PANIC.
create table ao_with_malformed_visimaprelid (a int) with (appendonly=true);
-- Set visimaprelid record in the pg_appendonly table to a malformed value.
set allow_system_table_mods=true;
update pg_appendonly
  set visimaprelid=16383
  where relid=(select oid from pg_class where relname='ao_with_malformed_visimaprelid');
select pg_table_size('ao_with_malformed_visimaprelid');
drop table ao_with_malformed_visimaprelid;

-- Also test pg_relation_size() in a query that selects from pg_class. It is a
-- very typical way to use the functions, so make sure it works. (A
-- plausible difference to the above scenarios would be that the function
-- might get executed on different nodes, for example.)
select pg_relation_size(oid) between 3000000 and 5000000 from pg_class where relname = 'heapsizetest'; -- 3637248
