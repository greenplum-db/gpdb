create table bitmapscan_bug as select '2008-02-01'::DATE AS DT, 
	case when j <= 96 
		then 0 
	when j<= 98 then 2 
	when j<= 99 then 3 
	when i % 1000 < 900 then 4 
	when i % 1000 < 800 then 5 
	when i % 1000 <= 998 then 5 else 6 
	end as ind, 
	(i*117-j)::bigint as s from generate_series(1,100) i, generate_series(1,100) j distributed randomly;

create table bitmapscan_bug2 as select * from bitmapscan_bug;

insert into bitmapscan_bug select DT + 1, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 2, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 3, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 4, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 5, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 6, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 7, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 8, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 9, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 10, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 11, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 12, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 13, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 14, ind, s from bitmapscan_bug2;
insert into bitmapscan_bug select DT + 15, ind, s from bitmapscan_bug2;

create index bitmapscan_bug_idx on bitmapscan_bug using bitmap (ind, dt);

vacuum analyze bitmapscan_bug;


create table mpp4593_bmbug (dt date, ind int, s bigint);
create index bmbug_idx on mpp4593_bmbug using bitmap(ind, dt);

-- Test Bitmap Indexscan
create table bm_test (a integer, b integer) distributed by (a);
insert into bm_test select a, a%25 from generate_series(1,100) a;
create index bm_test_a on bm_test (a);
set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

-- returns one or more tuples
select * from bm_test where a<10;

-- returns no tuples
select * from bm_test where a>100;

-- Test Bitmap Heapscan + Bitmap OR + Bitmap Indexscan

drop table if exists bm_test;
create table bm_test (a integer, b integer) distributed by (a);
insert into bm_test select a, a%25 from generate_series(1,100) a;

-- Test on 2 btrees
create index bm_test_a on bm_test (a);
create index bm_test_b on bm_test (b);

set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

select * from bm_test where a<100 or b>10;

-- Returns no tuples from one branch
select * from bm_test where a<100 or b>30;

-- Returns no tuples from both branch
select * from bm_test where a<1 or b>30;

-- Test on 2 bitmaps
drop index bm_test_a;
drop index bm_test_b;
create index bm_test_bm_a on bm_test using bitmap(a);
create index bm_test_bm_b on bm_test using bitmap(b);

select * from bm_test where a<100 or b>10;

-- Returns no tuples from one branch
select * from bm_test where a<100 or b>30;

-- Returns no tuples from both branch
select * from bm_test where a<1 or b>30;

-- Test on 1 btree, 1 bitmap
drop index bm_test_bm_a;
drop index bm_test_bm_b;
create index bm_test_a on bm_test (a);
create index bm_test_bm_b on bm_test using bitmap(b);

select * from bm_test where a<100 or b>10;

-- Returns no tuples from one branch
select * from bm_test where a<100 or b>30;

-- Returns no tuples from both branch
select * from bm_test where a<1 or b>30;

-- Test ArrayKeys
drop table if exists bm_test;
create table bm_test (a integer, b integer) distributed by (a);
insert into bm_test select a, a%25 from generate_series(1,100) a;
create index bm_test_a on bm_test (a);
set enable_seqscan=off;
set enable_indexscan=off;
set enable_bitmapscan=on;

select * from bm_test where a in (1,3,5);

drop table if exists bm_test;

-- Create a heap table.
CREATE TABLE card_heap_table_w_bitmap (id INTEGER, v VARCHAR) DISTRIBUTED BY (id);

-- Insert a few rows.
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (1, 
 'Water molecules cling because their electrostatic charge is polarized.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (2, 
 'The first law of thermodynamics is that matter and energy can neither be created nor destroyed.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (3, 
 'The second law of thermodynamics essentially says that you cannot recycle energy.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (4, 
 'The mass of the universe is finite and static.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (5, 
 'Population is growing exponentially.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (6, 
 'What happens next?');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (7, 
 'Fusion works by fusing 4 hydrogen atoms into 1 helium atom, or by fusing 2 deuterium atoms (deuterium is an isotope of hydrogen in which the nucleus contains a neutron).');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (8, 
 'Give a man a fish, and he will eat for a day.  Teach a man to fission, and he will blow up the planet for all eternity.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (9, 
 'Mercury, lead, and cadmium -- a day ago I had me some.  Now I feel really dense.');

-- Now force the values in the "v" column to use up more space.
UPDATE card_heap_table_w_bitmap SET v = v || v;
UPDATE card_heap_table_w_bitmap SET v = v || v;
UPDATE card_heap_table_w_bitmap SET v = v || v;
UPDATE card_heap_table_w_bitmap SET v = v || v;
UPDATE card_heap_table_w_bitmap SET v = v || v;
UPDATE card_heap_table_w_bitmap SET v = v || v;
UPDATE card_heap_table_w_bitmap SET v = v || v;
UPDATE card_heap_table_w_bitmap SET v = v || v;

-- Create a bitmap index.
CREATE INDEX card_heap_bitmap_idx1 ON card_heap_table_w_bitmap USING BITMAP (v);

-- Insert some more rows.
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (10, 
 'Rare earth metals are not the only rare metals in the earth.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (11, 
 'Who needs cinnabar when you have tuna?');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (12, 
 'This drunk walk cinnabar...');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (13, 
 'Hydrogen, helium, lithium.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (14, 
 'Hydrosphere, heliopause, lithosphere.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (15, 
 'Spelunking is not for the claustrophobic.');
INSERT INTO card_heap_table_w_bitmap (id, v) VALUES (16, 
 'Beam me up Spock.  There is NO intelligent life on this planet.');

-- Add column to the table.
-- The first column will have a low cardinality but a large domain.  There 
-- will be only a few distinct values in this column, but the values will 
-- cover a wide range (from -2147483548 to +2147483647.  Note that the 16
-- existing rows will get a value of NULL for this column. 
ALTER TABLE card_heap_table_w_bitmap ADD COLUMN lowCardinalityHighDomain INTEGER DEFAULT NULL;
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (
 17, 'Can we stop malaria by breeding a mosquito that cannot host malaria?', 
 -2147483647);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (
 18, 'Andes, Butte, Cascades, Denali, Everest', 
 0);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (
 19, 'Sawtooth, Sierras, Sangre de Cristos', 
 2147483647);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (
 20, 'Ganges, Brahmaputra, Indus',
 -2147483648);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (
 21, NULL, -2147483648);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (
 22, 'Amazon, Mad, Mississipi, Ohio, Sacramento, Merced', -2147483647);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (
 23, 'Yellow, Red, Green, Blue Nile, White Nile, denial', 
 -2147483647);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (24,
 'Earthquake supplies: water, sleeping bag, hand sanitizer', 
 0);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (25, 
 'radio, batteries, flashlight, camp stove', 2147483646);
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) VALUES (26, 
 'books, first aid equipment, axe, water purifier', 2147483647);

-- Insert enough rows to get us up to 10,000 rows.
INSERT INTO card_heap_table_w_bitmap (id, v, lowCardinalityHighDomain) 
 SELECT i, CAST(i AS VARCHAR), i % 100 FROM generate_series(27, 10000) i;

-- The following CREATE INDEX statements helps us test all of the following 
-- conditions:
--    a multi-column index.  
--    an index that contains columns that are also in another index.
--    a bitmap index on a column with a large domain but a small cardinality.
CREATE INDEX index2 ON card_heap_table_w_bitmap USING BITMAP (lowCardinalityHighDomain, v);

-- analyze the table
ANALYZE card_heap_table_w_bitmap;

-- Although we have 10,000 rows or more, the lowCardinalityHighDomain column 
-- has only about 104 distinct values: 0-99, -2147483648, -2147483647, 
-- 2147483647 and 2147483646.
SELECT COUNT(DISTINCT lowCardinalityHighDomain) FROM card_heap_table_w_bitmap;

-- There should be 99 rows with this value.
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain = 5;

-- Each of these tests a "single-sided range".
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain < 0;
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain > 100;

-- Select an individual row.
SELECT * FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain = 2147483646;

UPDATE card_heap_table_w_bitmap SET lowCardinalityHighDomain = NULL WHERE lowCardinalityHighDomain = 4;
SELECT COUNT(DISTINCT lowCardinalityHighDomain) FROM card_heap_table_w_bitmap;
-- There should be approximately 115 NULL values (99 that we just updated, 
-- and 16 original rows that got NULL when we added the column).
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain IS NULL;
-- There should no longer be any rows with value 4.
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain = 4;

-- We should have 10,000 rows now.
SELECT COUNT(*) FROM card_heap_table_w_bitmap;

-- This should delete 99 rows.
DELETE FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain = 3;

-- There should be 99 records like this.
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain = 22;

-- Now reduce the cardinality
DELETE FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain > 30
  AND lowCardinalityHighDomain <= 50 AND lowCardinalityHighDomain % 2 = 0;

SELECT COUNT(*) FROM card_heap_table_w_bitmap;
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain < 10;
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain > 100;
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain IS NULL;
-- The number of rows updated here should be equal to the total number of rows 
-- minus the number that have lowCardinalityHighDomain less than 10 
-- minus the number that have lowCardinalityHighDomain greater than 100
-- minus the number that are NULL (if any).
UPDATE card_heap_table_w_bitmap SET lowCardinalityHighDomain = 200 WHERE lowCardinalityHighDomain >= 10 and lowCardinalityHighDomain <= 100;

-- Should be around 14 rows.
SELECT DISTINCT lowCardinalityHighDomain FROM card_heap_table_w_bitmap;

REINDEX INDEX card_heap_bitmap_idx1;

-- Should still be around 14 rows.
SELECT DISTINCT lowCardinalityHighDomain FROM card_heap_table_w_bitmap;

-- There should be 99 records like this.
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain = 7;

-- There should be 7983 rows like this.
SELECT COUNT(*) FROM card_heap_table_w_bitmap WHERE lowCardinalityHighDomain = 200;

ALTER TABLE card_heap_table_w_bitmap RENAME COLUMN lowCardinalityHighDomain TO highCardinalityHighDomain;

-- Now add a lot more rows with few repeated values so that the 
-- cardinality becomes quite high (around 50,000 distinct values)
INSERT INTO card_heap_table_w_bitmap (id, v, highCardinalityHighDomain) 
 SELECT i, CAST(i AS VARCHAR), i % 50000 FROM generate_series(1000001, 1050000) i;

SELECT COUNT(DISTINCT(highCardinalityHighDomain)) AS distinct_hchd FROM card_heap_table_w_bitmap ORDER BY distinct_hchd;

drop table if exists bm_test;

-- Test if StreamNodes and StreamBitmaps are freed correctly.
create table bm_table_foo (c int, d int) distributed by (c);
create index ie_bm_table_foo on bm_table_foo using bitmap(d);
insert into bm_table_foo values (1, 1);
insert into bm_table_foo values (2, 2);

-- Next queries will create additional StreamNodes. In particular,
-- d in (1, 2) will be transformed into an OR with two BMS_INDEX input streams.
select * from bm_table_foo where d in (1, 2) and (d = 1 or d = 2);
select * from bm_table_foo where (d = 1 or d = 2) and d in (1, 2);

-- This query will eliminate StreamNodes since there is no tuple where d =3.
select * from bm_table_foo where d = 3 and (d = 1 or d = 2);

-- If a segment contains tuples with d in (1, 2), then it will create the whole StreamNode tree, 
-- otherwise segments will eliminate nodes.
select * from bm_table_foo where d = 2 and (d = 1 or d = 2);

-- double free mixed bitmap indexes (StreamBitmap with TIDBitmap)
CREATE TABLE bmheapcrash (
    btree_col2 date DEFAULT now(),
    bitmap_col text NOT NULL,
    btree_col1 character varying(50) NOT NULL,
    dist_col serial
)
DISTRIBUTED BY (dist_col);

CREATE INDEX bm_heap_idx ON bmheapcrash USING bitmap (bitmap_col);
CREATE INDEX bt_heap_idx ON bmheapcrash USING btree (btree_col1);
CREATE INDEX bt_heap_idx_2 ON bmheapcrash USING btree (btree_col2);

INSERT INTO bmheapcrash (btree_col2, bitmap_col, btree_col1)
SELECT date '2015-01-01' + (i % (365 * 2)), i % 1000, 'abcdefg' || (i% 1000)
from generate_series(1,10000) as i;

select count(1) from bmheapcrash where bitmap_col = '999' AND btree_col1 = 'abcdefg999';
select count(1) from bmheapcrash where bitmap_col = '999' OR btree_col1 = 'abcdefg999';
select count(1) from bmheapcrash where bitmap_col = '999' OR btree_col1 = 'abcdefg999' AND btree_col2 = '2015-01-01';
select count(1) from bmheapcrash where bitmap_col = '999' AND btree_col1 = 'abcdefg999' OR btree_col2 = '2015-01-01';

select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999';
select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' OR bitmap_col = '999' AND btree_col2 = '2015-01-01';
select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999' OR btree_col2 = '2015-01-01';
select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' AND btree_col2 = '2015-01-01' OR bitmap_col = '999';
select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' AND btree_col2 = '2015-01-01' AND bitmap_col = '999';
select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' OR btree_col2 = '2015-01-01' AND bitmap_col = '999';
select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' OR btree_col2 = '2015-01-01' OR bitmap_col = '999';

set enable_bitmapscan=on; 
set enable_hashjoin=off; 
set enable_indexscan=on; 
set enable_nestloop=on; 
set enable_seqscan=off;

select count(1) from bmheapcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999' OR bitmap_col = '888' OR btree_col2 = '2015-01-01';
select count(1) from bmheapcrash b1, bmheapcrash b2 where b1.bitmap_col = b2.bitmap_col or b1.bitmap_col = '999' and b1.btree_col1 = 'abcdefg999';

set optimizer_enable_hashjoin = off;
with bm as (select * from bmheapcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999' OR bitmap_col = '888' OR btree_col2 = '2015-01-01')
select count(1) from bm b1, bm b2 where b1.dist_col = b2.dist_col;

-- qual with like, any.
with bm as (select * from bmheapcrash where (btree_col1 like 'abcde%') AND bitmap_col in ('999', '888'))
select count(1) from bm b1, bm b2 where b1.dist_col = b2.dist_col;
