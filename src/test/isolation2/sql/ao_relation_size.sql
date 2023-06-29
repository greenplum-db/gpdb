-- Test on AO ROW tables with bloat, and with auxiliary tables included.  Check both physical and
-- logical size, check with seg0, and also check if rewrite removes the bloat
CREATE TABLE aorowsizetest (a int, col1 int, col2 text) WITH (appendonly=true, orientation=row);
INSERT INTO aorowsizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
1: BEGIN;
2: BEGIN;
3: BEGIN;
1: INSERT INTO aorowsizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
2: INSERT INTO aorowsizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
3: INSERT INTO aorowsizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
1: COMMIT;
2: COMMIT;
3: ABORT;
select pg_relation_size('aorowsizetest', /* include_ao_aux */ false, /* physical_ao_size */ true) between 10000000 and 10500000;
-- 10411040
select pg_relation_size('aorowsizetest', /* include_ao_aux */ true, /* physical_ao_size */ true) > pg_relation_size('aorowsizetest', /* include_ao_aux */ false, /* physical_ao_size */ true);
select pg_relation_size('aorowsizetest', /* include_ao_aux */ true, /* physical_ao_size */ true) = pg_relation_size('aorowsizetest', /* include_ao_aux */ true, /* physical_ao_size */ false);
select pg_table_size('aorowsizetest') between 10500000 and 11000000;
-- 10771488
select pg_table_size('aorowsizetest') = pg_relation_size('aorowsizetest', /* include_ao_aux */ true, /* physical_ao_size */ true);
select pg_total_relation_size('aorowsizetest') = pg_table_size('aorowsizetest');
-- Test seg0
1: BEGIN;
1: ALTER TABLE aorowsizetest SET WITH (reorganize = 'true');
1: INSERT INTO aorowsizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
1: COMMIT;
SELECT * FROM gp_toolkit.__gp_aoseg('aorowsizetest') ORDER BY segment_id;
select pg_relation_size('aorowsizetest', /* include_ao_aux */ false, /* physical_ao_size */ true) between 10000000 and 10500000;
-- 10410920
select pg_relation_size('aorowsizetest', /* include_ao_aux */ false, /* physical_ao_size */ false) between 10000000 and 10500000;
-- 10410920
select pg_relation_size('aorowsizetest', /* include_ao_aux */ true, /* physical_ao_size */ true) > pg_relation_size('aorowsizetest', /* include_ao_aux */ false, /* physical_ao_size */ true);
select pg_table_size('aorowsizetest') between 10500000 and 11000000;
-- 10771368
select pg_table_size('aorowsizetest') = pg_relation_size('aorowsizetest', /* include_ao_aux */ true, /* physical_ao_size */ true);
select pg_total_relation_size('aorowsizetest') = pg_table_size('aorowsizetest');


-- Test on AOCO tables with bloat, and with auxiliary tables included.  Check both physical and
-- logical size, check with seg0, and also check if rewrite removes the bloat
CREATE TABLE aocssizetest (a int, col1 int, col2 text) WITH (appendonly=true, orientation=column);
INSERT INTO aocssizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
1: BEGIN;
2: BEGIN;
3: BEGIN;
1: INSERT INTO aocssizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
2: INSERT INTO aocssizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
3: INSERT INTO aocssizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
1: COMMIT;
2: COMMIT;
3: ABORT;
select pg_relation_size('aocssizetest', /* include_ao_aux */ false, /* physical_ao_size */ true) between 5000000 and 6000000;
-- 5964800
select pg_relation_size('aocssizetest', /* include_ao_aux */ true, /* physical_ao_size */ true) > pg_relation_size('aocssizetest', /* include_ao_aux */ false, /* physical_ao_size */ true);
select pg_relation_size('aocssizetest', /* include_ao_aux */ true, /* physical_ao_size */ true) = pg_relation_size('aocssizetest', /* include_ao_aux */ true, /* physical_ao_size */ false);
select pg_table_size('aocssizetest') between 4000000 and 5000000;
-- 6194176
select pg_table_size('aocssizetest') = pg_relation_size('aocssizetest', /* include_ao_aux */ true, /* physical_ao_size */ true);
select pg_total_relation_size('aocssizetest') = pg_table_size('aocssizetest');
-- Test seg0
1: BEGIN;
1: ALTER TABLE aocssizetest SET WITH (reorganize = 'true');
1: INSERT INTO aocssizetest SELECT g, g, 'x' || g FROM generate_series(1, 100000) g;
1: COMMIT;
SELECT * FROM gp_toolkit.__gp_aocsseg('aocssizetest') ORDER BY segment_id, column_num;
select pg_relation_size('aocssizetest', /* include_ao_aux */ false, /* physical_ao_size */ true) between 5500000 and 6500000;
-- 5964112
select pg_relation_size('aocssizetest', /* include_ao_aux */ false, /* physical_ao_size */ false) between 1000000 and 2000000;
-- 5964112
select pg_relation_size('aocssizetest', /* include_ao_aux */ true, /* physical_ao_size */ true) > pg_relation_size('aocssizetest', /* include_ao_aux */ false, /* physical_ao_size */ true);
select pg_table_size('aocssizetest') between 2500000 and 3500000;
-- 3015168
select pg_table_size('aocssizetest') = pg_relation_size('aocssizetest', /* include_ao_aux */ true, /* physical_ao_size */ true);
select pg_total_relation_size('aocssizetest') = pg_table_size('aocssizetest');