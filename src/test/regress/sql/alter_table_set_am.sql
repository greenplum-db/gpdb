-- Check changing table access method

\set HIDE_TABLEAM off

-- Scenario 1: Changing to the same AM: it should have no effect but
-- make sure it doesn't rewrite table or blow up existing reloptions:
CREATE TABLE sameam_heap(a int, b int) WITH (fillfactor=70) DISTRIBUTED BY (a);
CREATE TABLE sameam_heap2(a int, b int) WITH (fillfactor=70) DISTRIBUTED BY (a);
CREATE TABLE sameam_ao(a int, b int) WITH (appendoptimized=true, orientation=row, compresstype=zlib, compresslevel=3);
CREATE TABLE sameam_co(a int, b int) WITH (appendoptimized=true, orientation=column, compresstype=rle_type, compresslevel=3);

INSERT INTO sameam_heap SELECT i,i FROM generate_series(1,5) i;
INSERT INTO sameam_heap2 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO sameam_ao SELECT i,i FROM generate_series(1,5) i;
INSERT INTO sameam_co SELECT i,i FROM generate_series(1,5) i;

CREATE TEMP TABLE relfilebeforesameam AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'sameam_%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'sameam_%' ORDER BY segid;

-- changing to the same access method shouldn't rewrite the table
-- (i.e. the relfilenodes shouldn't change)
ALTER TABLE sameam_heap SET ACCESS METHOD heap;
ALTER TABLE sameam_heap2 SET WITH (appendoptimized=false); -- Alternative syntax of ATSETAM
ALTER TABLE sameam_ao SET ACCESS METHOD ao_row;
ALTER TABLE sameam_co SET ACCESS METHOD ao_column;

CREATE TEMP TABLE relfileaftersameam AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'sameam_%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'sameam_%' ORDER BY segid;

-- relfilenodes shouldn't change
SELECT * FROM relfilebeforesameam EXCEPT SELECT * FROM relfileaftersameam;
-- reloptions should remain the same
SELECT relname, reloptions FROM pg_class WHERE relname LIKE 'sameam_%';

-- Scenario 2: Heap to AO
CREATE TABLE heap2ao(a int, b int) WITH (fillfactor=70) DISTRIBUTED BY (a);
CREATE TABLE heap2ao2(a int, b int) DISTRIBUTED BY (a);

CREATE INDEX heapi ON heap2ao(b);
ALTER TABLE heap2ao2 ADD CONSTRAINT unique_constraint UNIQUE (a);

INSERT INTO heap2ao SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heap2ao2 SELECT i,i FROM generate_series(1,5) i;

-- Check reloptions once before altering.
SELECT reloptions from pg_class where relname in ('heap2ao', 'heap2ao2');

CREATE TEMP TABLE relfilebeforeao AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heap2ao', 'heap2ao2', 'heapi')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heap2ao', 'heap2ao2', 'heapi') ORDER BY segid;

-- Set default storage options for the table to inherit from
SET gp_default_storage_options = 'blocksize=65536, compresstype=zlib, compresslevel=5, checksum=true';

-- Alter table heap to AO should work, even if heap table has unique indexes.
ALTER TABLE heap2ao SET ACCESS METHOD ao_row;
ALTER TABLE heap2ao2 SET WITH (appendoptimized=true);

-- The altered tables should have AO AM
-- The altered tables should inherit storage options from gp_default_storage_options
-- And, the original heap reloptions are gone (in this case, 'fillfactor').
-- Also, equivalent indexes have been created
\d+ heap2ao
\d+ heap2ao2

-- Check reloptions
SELECT relname, reloptions from pg_class where relname in ('heap2ao', 'heap2ao2');

-- Check data is intact
SELECT * FROM heap2ao;
SELECT * FROM heap2ao2;

-- The tables and indexes should have been rewritten (should have different relfilenodes)
CREATE TEMP TABLE relfileafterao AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heap2ao', 'heap2ao2', 'heapi')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heap2ao', 'heap2ao2', 'heapi') ORDER BY segid;

SELECT * FROM relfilebeforeao INTERSECT SELECT * FROM relfileafterao;

-- aux tables are created, pg_appendonly row is created
SELECT * FROM gp_toolkit.__gp_aoseg('heap2ao');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heap2ao')).* FROM gp_dist_random('gp_id');
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('heap2ao')).* FROM gp_dist_random('gp_id');

SELECT * FROM gp_toolkit.__gp_aoseg('heap2ao2');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heap2ao2')).* FROM gp_dist_random('gp_id');

-- pg_appendonly should have entries associated with the new AO tables
SELECT c.relname FROM pg_class c, pg_appendonly p WHERE c.relname LIKE 'heap2ao%' AND c.oid = p.relid;

-- check inherited tables
CREATE TABLE heapbase (a int, b int);
CREATE TABLE heapchild (c int) INHERITS (heapbase);
CREATE TABLE heapbase2 (a int, b int);
CREATE TABLE heapchild2 (c int) INHERITS (heapbase2);

INSERT INTO heapbase SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heapchild SELECT i,i,i FROM generate_series(1,5) i;
INSERT INTO heapbase2 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heapchild2 SELECT i,i,i FROM generate_series(1,5) i;

CREATE TEMP TABLE inheritrelfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heapbase', 'heapbase2')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heapbase', 'heapbase2') ORDER BY segid;

CREATE TEMP TABLE inheritchildrelfilebefore AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heapchild', 'heapchild2')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heapchild', 'heapchild2') ORDER BY segid;

ALTER TABLE heapbase SET ACCESS METHOD ao_row;
ALTER TABLE heapbase2 SET WITH (appendoptimized=true);

-- The altered tables should inherit storage options from gp_default_storage_options
show gp_default_storage_options;
SELECT reloptions from pg_class where relname in ('heapbase','heapbase2');

SELECT reloptions from pg_class where relname in ('heapchild','heapchild2');

-- The altered parent tables should have AO AM but child tables are still heap
SELECT c.relname, a.amname FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'heapbase%' OR c.relname LIKE 'heapchild%';

-- Check data is intact
SELECT * FROM heapbase;
SELECT * FROM heapbase2;

-- relfile node should change for base table set to AO
CREATE TEMP TABLE inheritrelfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heapbase', 'heapbase2')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heapbase', 'heapbase2') ORDER BY segid;

SELECT * FROM inheritrelfilebefore INTERSECT SELECT * FROM inheritrelfileafter;

-- relfile node should not change for child table
CREATE TEMP TABLE inheritchildrelfileafter AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heapchild', 'heapchild2')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heapchild', 'heapchild2') ORDER BY segid;

SELECT count(*) FROM (SELECT * FROM inheritchildrelfilebefore UNION SELECT * FROM inheritchildrelfileafter)a;

-- aux tables are created, pg_appendonly row is created
SELECT * FROM gp_toolkit.__gp_aoseg('heapbase');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heapbase')).* FROM gp_dist_random('gp_id');
SELECT * FROM gp_toolkit.__gp_aoseg('heapbase2');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heapbase2')).* FROM gp_dist_random('gp_id');
SELECT c.relname FROM pg_class c, pg_appendonly p WHERE c.relname LIKE 'heapbase%' AND c.oid = p.relid;

-- aux tables are not created for child table
SELECT * FROM gp_toolkit.__gp_aoseg('heapchild');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heapchild')).* FROM gp_dist_random('gp_id');
SELECT * FROM gp_toolkit.__gp_aoseg('heapchild2');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heapchild2')).* FROM gp_dist_random('gp_id');
SELECT c.relname FROM pg_class c, pg_appendonly p WHERE c.relname LIKE 'heapchild%' AND c.oid = p.relid;

-- Scenario 3: AO to Heap
SET gp_default_storage_options = 'blocksize=65536, compresstype=zlib, compresslevel=5, checksum=true';

CREATE TABLE ao2heap(a int, b int) WITH (appendonly=true);
CREATE TABLE ao2heap2(a int, b int) WITH (appendonly=true);
CREATE INDEX aoi ON ao2heap(b);

INSERT INTO ao2heap SELECT i,i FROM generate_series(1,5) i;
INSERT INTO ao2heap2 SELECT i,i FROM generate_series(1,5) i;

-- Check once that the AO tables have the custom reloptions 
SELECT relname, reloptions FROM pg_class WHERE relname LIKE 'ao2heap%';

-- Check once that the AO tables have relfrozenxid = 0
SELECT relname, relfrozenxid FROM pg_class WHERE relname LIKE 'ao2heap%';

CREATE TEMP TABLE relfilebeforeao2heap AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('ao2heap', 'ao2heap2', 'aoi')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('ao2heap', 'ao2heap2', 'aoi') ORDER BY segid;

-- Altering AO to heap
ALTER TABLE ao2heap SET ACCESS METHOD heap;
ALTER TABLE ao2heap2 SET WITH (appendoptimized=false);

-- The tables and indexes should have been rewritten (should have different relfilenodes)
CREATE TEMP TABLE relfileafterao2heap AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('ao2heap', 'ao2heap2', 'aoi')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('ao2heap', 'ao2heap2', 'aoi') ORDER BY segid;

SELECT * FROM relfilebeforeao2heap INTERSECT SELECT * FROM relfileafterao2heap;

-- Check data is intact
SELECT * FROM ao2heap;
SELECT * FROM ao2heap2;

-- No AO aux tables should be left
SELECT * FROM gp_toolkit.__gp_aoseg('ao2heap');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('ao2heap')).* FROM gp_dist_random('gp_id');
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('ao2heap')).* FROM gp_dist_random('gp_id');
SELECT * FROM gp_toolkit.__gp_aoseg('ao2heap2');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('ao2heap2')).* FROM gp_dist_random('gp_id');

-- No pg_appendonly entries should be left too
SELECT c.relname FROM pg_class c, pg_appendonly p WHERE c.relname LIKE 'ao2heap%' AND c.oid = p.relid;

-- The altered tables should have heap AM.
SELECT c.relname, a.amname FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'ao2heap%';

-- The new heap tables shouldn't have the old AO table's reloptions
SELECT relname, reloptions FROM pg_class WHERE relname LIKE 'ao2heap%';

-- The new heap tables should have a valid relfrozenxid
SELECT relname, relfrozenxid <> '0' FROM pg_class WHERE relname LIKE 'ao2heap%';

DROP TABLE ao2heap;
DROP TABLE ao2heap2;

-- Scenario 4: Set reloptions along with change of AM.

CREATE TABLE ataoset(a int);
CREATE TABLE ataoset2(a int);
INSERT INTO ataoset select * from generate_series(1, 5);
INSERT INTO ataoset2 select * from generate_series(1, 5);

-- Error: user specifies a different AM than the one indicated in the WITH clause
ALTER TABLE ataoset SET ACCESS METHOD ao_row WITH(appendonly=true, orientation=column);

-- Error: user specifiies AO reloption when altering an AO table to heap.
CREATE TABLE ao2heaperror (a int) WITH (appendonly=true);
ALTER TABLE ao2heaperror SET ACCESS METHOD heap WITH (blocksize=65536);
DROP TABLE ao2heaperror;

-- Scenario 4.1: change from heap to AO with customized storage options
CREATE TEMP TABLE relfilebeforeat AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

ALTER TABLE ataoset SET WITH (appendonly=true, blocksize=65536, compresslevel=7);
ALTER TABLE ataoset2 SET ACCESS METHOD ao_row WITH (blocksize=65536, compresslevel=7);

CREATE TEMP TABLE relfileafterat AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

-- relfilenode changed
SELECT * FROM relfilebeforeat INTERSECT SELECT * FROM relfileafterat;

DROP TABLE relfilebeforeat;
DROP TABLE relfileafterat;

-- table AMs are changed to AO, and reloptions changed to what we set
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'ataoset%';

-- data are intact
SELECT count(*) FROM ataoset;
SELECT count(*) FROM ataoset2;

-- Scenario 4.2. Alter the table w/ the exact same reloptions. 
-- AM, relfilenodes and reloptions all should remain the same
CREATE TEMP TABLE relfilebeforeat AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

-- Firstly alter them with the exact same options as case 1.
-- Secondly alter them with same option values but different order.
-- Neither should trigger table rewrite. 
ALTER TABLE ataoset SET WITH (appendonly=true, blocksize=65536, compresslevel=7);
ALTER TABLE ataoset SET WITH (appendonly=true, compresslevel=7, blocksize=65536);
ALTER TABLE ataoset2 SET ACCESS METHOD ao_row WITH (blocksize=65536, compresslevel=7);
ALTER TABLE ataoset2 SET ACCESS METHOD ao_row WITH (compresslevel=7, blocksize=65536);

CREATE TEMP TABLE relfileafterat AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

-- no change to relfilenode
SELECT * FROM relfilebeforeat EXCEPT SELECT * FROM relfileafterat;

DROP TABLE relfilebeforeat;
DROP TABLE relfileafterat;

-- table AMs are still AO, but reloptions should reflect the order of options in the most recent ALTER TABLE
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'ataoset%';

-- data still intact
SELECT count(*) FROM ataoset;
SELECT count(*) FROM ataoset2;

-- Scenario 4.3. Use the same syntax to alter from AO to AO, but specifying different storage options.
--         Table should be rewritten.
CREATE TEMP TABLE relfilebeforeao AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

ALTER TABLE ataoset SET WITH (appendonly=true, blocksize=32768);
ALTER TABLE ataoset2 SET ACCESS METHOD ao_row WITH (blocksize=32768);

CREATE TEMP TABLE relfileafterao AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

-- table is rewritten
SELECT * FROM relfilebeforeao INTERSECT SELECT * FROM relfileafterao;

-- reloptions changed too
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'ataoset%';

DROP TABLE relfilebeforeao;
DROP TABLE relfileafterao;

-- Scenario 4.4. Alter the tables back to heap and set some reloptions too.
CREATE TEMP TABLE relfilebeforeat AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

ALTER TABLE ataoset SET ACCESS METHOD heap WITH (fillfactor=70);
ALTER TABLE ataoset2 SET ACCESS METHOD heap WITH (fillfactor=70);

CREATE TEMP TABLE relfileafterat AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ataoset%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ataoset%' ORDER BY segid;

-- there's a table rewrite
SELECT * FROM relfilebeforeat INTERSECT SELECT * FROM relfileafterat;

DROP TABLE relfilebeforeat;
DROP TABLE relfileafterat;

-- reloptions and AM also changed
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'ataoset%';

DROP TABLE ataoset;
DROP TABLE ataoset2;

-- Scenario 5: AO to AOCO 
SET gp_default_storage_options = 'blocksize=65536, compresstype=zlib, compresslevel=5, checksum=true';
CREATE TABLE ao2co(a int, b int) WITH (appendonly=true);
CREATE TABLE ao2co2(a int, b int) WITH (appendonly=true);
CREATE TABLE ao2co3(a int, b int) WITH (appendonly=true);
CREATE TABLE ao2co4(a int, b int) WITH (appendonly=true);
CREATE INDEX index_ao2co ON ao2co(b);
CREATE INDEX index_ao2co3 ON ao2co3(b);

INSERT INTO ao2co SELECT i,i FROM generate_series(1,5) i;
INSERT INTO ao2co2 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO ao2co3 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO ao2co4 SELECT i,i FROM generate_series(1,5) i;

-- ERROR: conflicting storage option specified.
ALTER TABLE ao2co SET ACCESS METHOD ao_column WITH (appendoptimized=true, orientation=row);
-- Use of *both* ACCESS METHOD and WITH clauses is allowed, but we'll print a hint to indicate the redundancy.
ALTER TABLE ao2co SET ACCESS METHOD ao_row WITH (appendoptimized=true, orientation=row);

CREATE TEMP TABLE relfilebeforeao AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ao2co%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ao2co%' ORDER BY segid;

-- Check once the reloptions
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'ao2co%';

-- Altering AO to AOCO with various syntaxes, reloptions:
ALTER TABLE ao2co SET ACCESS METHOD ao_column;
ALTER TABLE ao2co2 SET WITH (appendoptimized=true, orientation=column);
ALTER TABLE ao2co3 SET ACCESS METHOD ao_column WITH (blocksize=32768, compresstype=rle_type, compresslevel=3);
ALTER TABLE ao2co4 SET WITH (appendoptimized=true, orientation=column, blocksize=32768, compresstype=rle_type, compresslevel=3);

-- The tables are rewritten
CREATE TEMP TABLE relfileafterao AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'ao2co%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'ao2co%' ORDER BY segid;

SELECT * FROM relfilebeforeao INTERSECT SELECT * FROM relfileafterao;
DROP TABLE relfilebeforeao;
DROP TABLE relfileafterao;

-- Check data is intact
SELECT count(*) FROM ao2co;
SELECT count(*) FROM ao2co2;
SELECT count(*) FROM ao2co3;
SELECT count(*) FROM ao2co4;

-- Aux tables should have been deleted for the old AO table and recreated for the new AOCO table
-- Only tested for 2 out of the 4 tables being created, where the tables were altered w/wo reloptions. 
-- No need to test the other ones created by the alternative syntax SET WITH().
SELECT * FROM gp_toolkit.__gp_aoseg('ao2co');
SELECT * FROM gp_toolkit.__gp_aovisimap('ao2co');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg('ao2co');
SELECT * FROM gp_toolkit.__gp_aoblkdir('ao2co');
SELECT * FROM gp_toolkit.__gp_aoseg('ao2co3');
SELECT * FROM gp_toolkit.__gp_aovisimap('ao2co3');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg('ao2co3');
SELECT * FROM gp_toolkit.__gp_aoblkdir('ao2co3');

-- pg_attribute_encoding should have columns for the AOCO table
SELECT c.relname, a.attnum, a.filenum, a.attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid = c.oid AND c.relname LIKE 'ao2co%';

-- AM and reloptions changed accordingly
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'ao2co%';

DROP TABLE ao2co;
DROP TABLE ao2co2;
DROP TABLE ao2co3;
DROP TABLE ao2co4;

-- Scenario 6: AOCO to Heap
SET gp_default_storage_options = 'blocksize=65536, compresstype=zlib, compresslevel=5, checksum=true';

CREATE TABLE co2heap(a int, b int) WITH (appendonly=true, orientation=column);
CREATE TABLE co2heap2(a int, b int) WITH (appendonly=true, orientation=column);
CREATE TABLE co2heap3(a int, b int) WITH (appendonly=true, orientation=column);
CREATE TABLE co2heap4(a int, b int) WITH (appendonly=true, orientation=column);
CREATE INDEX aoi ON co2heap(b);

INSERT INTO co2heap SELECT i,i FROM generate_series(1,5) i;
INSERT INTO co2heap2 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO co2heap3 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO co2heap4 SELECT i,i FROM generate_series(1,5) i;

-- Prior-ATSETAM checks:
-- Check once that the AO tables have the custom reloptions
SELECT relname, reloptions FROM pg_class WHERE relname LIKE 'co2heap%';
-- Check once that the AO tables have relfrozenxid = 0
SELECT relname, relfrozenxid FROM pg_class WHERE relname LIKE 'co2heap%';
-- Check once that the pg_attribute_encoding has entries for the AOCO tables.
SELECT c.relname, a.attnum, a.filenum, attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid=c.oid AND c.relname LIKE 'co2heap%';

CREATE TEMP TABLE relfilebeforeco2heap AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'co2heap%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'co2heap%' ORDER BY segid;

-- Various cases of altering AOCO to AO:
-- 1. Basic ATSETAMs:
ALTER TABLE co2heap SET ACCESS METHOD heap;
ALTER TABLE co2heap2 SET WITH (appendoptimized=false);
-- 2. ATSETAM with reloptions:
ALTER TABLE co2heap3 SET ACCESS METHOD heap WITH (fillfactor=70);
ALTER TABLE co2heap4 SET WITH (appendoptimized=false, fillfactor=70);

-- The tables and indexes should have been rewritten (should have different relfilenodes)
CREATE TEMP TABLE relfileafterco2heap AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'co2heap%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'co2heap%' ORDER BY segid;

SELECT * FROM relfilebeforeco2heap INTERSECT SELECT * FROM relfileafterco2heap;

-- Check data is intact
SELECT count(*) FROM co2heap;
SELECT count(*) FROM co2heap2;
SELECT count(*) FROM co2heap3;
SELECT count(*) FROM co2heap4;

-- No AO aux tables should be left.
-- Only testing 2 out of the 4 tables being created, where the tables were altered w/wo reloptions.
-- No need to test the other ones created by the alternative syntax SET WITH().
SELECT * FROM gp_toolkit.__gp_aoseg('co2heap');
SELECT * FROM gp_toolkit.__gp_aovisimap('co2heap');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg('co2heap');
SELECT * FROM gp_toolkit.__gp_aoblkdir('co2heap');
SELECT * FROM gp_toolkit.__gp_aoseg('co2heap3');
SELECT * FROM gp_toolkit.__gp_aovisimap('co2heap3');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg('co2heap3');
SELECT * FROM gp_toolkit.__gp_aoblkdir('co2heap3');

-- No pg_appendonly entries should be left too
SELECT c.relname FROM pg_class c, pg_appendonly p WHERE c.relname LIKE 'co2heap%' AND c.oid = p.relid;

-- The altered tables should have heap AM.
SELECT c.relname, a.amname FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'co2heap%';

-- The new heap tables shouldn't have the old AO table's reloptions
SELECT relname, reloptions FROM pg_class WHERE relname LIKE 'co2heap%';

-- The new heap tables should have a valid relfrozenxid
SELECT relname, relfrozenxid <> '0' FROM pg_class WHERE relname LIKE 'co2heap%';

-- The pg_attribute_encoding entries for the altered tables should have all gone.
SELECT c.relname, a.attnum, a.filenum, attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid=c.oid AND c.relname LIKE 'co2heap%';

DROP TABLE co2heap;
DROP TABLE co2heap2;
DROP TABLE co2heap3;
DROP TABLE co2heap4;

-- Scenario 7: AOCO to AO
CREATE TABLE co2ao(a int, b int) WITH (appendonly=true, orientation=column, compresstype=rle_type, compresslevel=3);
CREATE TABLE co2ao2(a int, b int) WITH (appendonly=true, orientation=column, compresstype=rle_type, compresslevel=3);
CREATE TABLE co2ao3(a int, b int) WITH (appendonly=true, orientation=column, compresstype=rle_type, compresslevel=3);
CREATE TABLE co2ao4(a int, b int) WITH (appendonly=true, orientation=column, compresstype=rle_type, compresslevel=3);
CREATE INDEX aoi ON co2ao(b);
CREATE INDEX aoi2 ON co2ao3(b);

INSERT INTO co2ao SELECT i,i FROM generate_series(1,5) i;
INSERT INTO co2ao2 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO co2ao3 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO co2ao4 SELECT i,i FROM generate_series(1,5) i;

-- Prior-ATSETAM checks:
-- Check once that the AOCO tables have the custom reloptions
SELECT relname, reloptions FROM pg_class WHERE relname LIKE 'co2ao%';
-- Check once that the pg_attribute_encoding has entries for the AOCO tables.
SELECT c.relname, a.attnum, a.filenum, attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid=c.oid AND c.relname LIKE 'co2ao%';
-- Check once on the aoblkdirs
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('co2ao')).* FROM gp_dist_random('gp_id');
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('co2ao3')).* FROM gp_dist_random('gp_id');

CREATE TEMP TABLE relfilebeforeco2ao AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'co2ao%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'co2ao%' ORDER BY segid;

-- Various cases of altering AOCO to AO:
-- 1. Basic ATSETAMs:
ALTER TABLE co2ao SET ACCESS METHOD ao_row;
ALTER TABLE co2ao2 SET WITH (appendoptimized=true);
-- 2. ATSETAM with reloptions:
ALTER TABLE co2ao3 SET ACCESS METHOD ao_row WITH (compresstype=zlib, compresslevel=7);
ALTER TABLE co2ao4 SET WITH (appendoptimized=true, compresstype=zlib, compresslevel=7);

-- The tables and indexes should have been rewritten (should have different relfilenodes)
CREATE TEMP TABLE relfileafterco2ao AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname LIKE 'co2ao%'
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'co2ao%' ORDER BY segid;

SELECT * FROM relfilebeforeco2ao INTERSECT SELECT * FROM relfileafterco2ao;

DROP TABLE relfilebeforeco2ao;
DROP TABLE relfileafterco2ao;

-- Check data is intact
SELECT count(*) FROM co2ao;
SELECT count(*) FROM co2ao2;
SELECT count(*) FROM co2ao3;
SELECT count(*) FROM co2ao4;

-- AO aux tables should still be there, but AOCO seg tables are not.
-- Only testing 2 out of the 4 tables being created, where the tables were altered w/wo reloptions.
-- No need to test the other ones created by the alternative syntax SET WITH().
SELECT * FROM gp_toolkit.__gp_aoseg('co2ao');
SELECT * FROM gp_toolkit.__gp_aocsseg('co2ao');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('co2ao')).* FROM gp_dist_random('gp_id');
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('co2ao')).* FROM gp_dist_random('gp_id');
SELECT * FROM gp_toolkit.__gp_aoseg('co2ao3');
SELECT * FROM gp_toolkit.__gp_aocsseg('co2ao3');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('co2ao3')).* FROM gp_dist_random('gp_id');
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('co2ao3')).* FROM gp_dist_random('gp_id');

-- The altered tables should show AO AM.
SELECT c.relname, a.amname FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'co2ao%';

-- Only the new tables altered w/ reloptions supplies should have reloptions.
SELECT relname, reloptions FROM pg_class WHERE relname LIKE 'co2ao%';

-- The pg_attribute_encoding entries for the altered tables should have all gone.
SELECT c.relname, a.attnum, a.filenum, attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid=c.oid AND c.relname LIKE 'co2ao%';

DROP TABLE co2ao;
DROP TABLE co2ao2;
DROP TABLE co2ao3;
DROP TABLE co2ao4;

-- Scenario 8: Heap to AOCO
SET gp_default_storage_options = 'blocksize=65536, compresstype=zlib, compresslevel=5, checksum=true';
CREATE TABLE heap2co(a int, b int);
CREATE TABLE heap2co2(a int, b int);
CREATE TABLE heap2co3(a int, b int);
CREATE TABLE heap2co4(a int, b int);
CREATE INDEX index_heap2co ON heap2co(b);
CREATE INDEX index_heap2co3 ON heap2co3(b);

INSERT INTO heap2co SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heap2co2 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heap2co3 SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heap2co4 SELECT i,i FROM generate_series(1,5) i;

CREATE TEMP TABLE relfilebeforeaoco AS
SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'heap2co%'
UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
WHERE relname LIKE 'heap2co%' ORDER BY segid;

-- ERROR: conflicting storage option specified.
ALTER TABLE heap2co SET ACCESS METHOD ao_column WITH (appendoptimized=false);
-- Use of *both* ACCESS METHOD and WITH clauses is allowed, but we'll print a hint to indicate the redundancy.
ALTER TABLE heap2co SET ACCESS METHOD ao_column WITH (appendoptimized=true, orientation=column);

-- Check once the reloptions
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'heap2co%';

-- Altering AO to AOCO with various syntaxes, reloptions:
ALTER TABLE heap2co SET ACCESS METHOD ao_column;
ALTER TABLE heap2co2 SET WITH (appendoptimized=true, orientation=column);
ALTER TABLE heap2co3 SET ACCESS METHOD ao_column WITH (blocksize=32768, compresslevel=3);
ALTER TABLE heap2co4 SET WITH (appendoptimized=true, orientation=column, blocksize=32768, compresslevel=3);

-- The tables are rewritten
CREATE TEMP TABLE relfileafteraoco AS
SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'heap2co%'
UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
WHERE relname LIKE 'heap2co%' ORDER BY segid;

SELECT * FROM relfilebeforeaoco INTERSECT SELECT * FROM relfileafteraoco;
DROP TABLE relfilebeforeaoco;
DROP TABLE relfileafteraoco;

-- Check data is intact
SELECT count(*) FROM heap2co;
SELECT count(*) FROM heap2co2;
SELECT count(*) FROM heap2co3;
SELECT count(*) FROM heap2co4;

-- Aux tables should have been created for the new AOCO table
-- Only tested for 2 out of the 4 tables being created, where the tables were altered w/wo reloptions.
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heap2co')).* FROM gp_dist_random('gp_id');
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('heap2co')).* FROM gp_dist_random('gp_id');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg('heap2co');
SELECT gp_segment_id, (gp_toolkit.__gp_aovisimap('heap2co3')).* FROM gp_dist_random('gp_id');
SELECT gp_segment_id, (gp_toolkit.__gp_aoblkdir('heap2co3')).* FROM gp_dist_random('gp_id');
SELECT count(*) FROM gp_toolkit.__gp_aocsseg('heap2co3');

-- pg_attribute_encoding should have columns for the AOCO table
SELECT c.relname, a.attnum, a.filenum, a.attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid = c.oid AND c.relname LIKE 'heap2co%';

-- AM and reloptions changed accordingly
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'heap2co%';

DROP TABLE heap2co;
DROP TABLE heap2co2;
DROP TABLE heap2co3;
DROP TABLE heap2co4;

-- Scenario 9. Altering partition tables
-- Altering the storage model at the partition root should change the storage model for all existing children,
-- including all the subpartitions.
-- FIXME: currently children tables aren't inheriting reloptions of the root. After that is corrected, we 
-- should add a corresponding test case for ALTER TABLE here too.
CREATE TABLE atsetam_part(a int, b int) PARTITION BY RANGE(a);
CREATE TABLE atsetam_part_1 partition OF atsetam_part FOR VALUES FROM (1) to (100);
CREATE TABLE atsetam_part_2 partition OF atsetam_part FOR VALUES FROM (100) to (200) PARTITION BY RANGE(a);
CREATE TABLE atsetam_part_2_1 partition OF atsetam_part_2 FOR VALUES FROM (100) to (150);
CREATE TABLE atsetam_part_2_2 partition OF atsetam_part_2 FOR VALUES FROM (150) to (200);

ALTER TABLE atsetam_part SET ACCESS METHOD ao_row WITH (blocksize=65536, compresslevel=7);

SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'atsetam_part%';

-- altering it again with the same AM and reloptions. Nothing should change.
ALTER TABLE atsetam_part SET ACCESS METHOD ao_row WITH (blocksize=65536, compresslevel=7);
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'atsetam_part%';

-- altering a subpartition root, should apply and only apply to the subpartition root/child.
ALTER TABLE atsetam_part_2 SET ACCESS METHOD ao_column WITH (compresstype=rle_type, compresslevel=3);
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'atsetam_part%';

-- attaching an existing table to the partition and the table should keep its AM.
CREATE TABLE atsetam_part_att(a int, b int); -- default is heap
ALTER TABLE atsetam_part ATTACH PARTITION atsetam_part_att FOR VALUES FROM (1000) to (1100);
SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname = 'atsetam_part_att';

DROP TABLE atsetam_part;

-- Altering the storage model at the partition root with the ONLY keyword should change the 
-- storage model for all future children. Existing children should remain unaffected.
CREATE TABLE atsetam_part(a int, b int) PARTITION BY RANGE(a);
CREATE TABLE atsetam_part_1 partition OF atsetam_part FOR VALUES FROM (1) to (100);
CREATE TABLE atsetam_part_2 partition OF atsetam_part FOR VALUES FROM (100) to (200) PARTITION BY RANGE(a);
CREATE TABLE atsetam_part_2_1 partition OF atsetam_part_2 FOR VALUES FROM (100) to (150);

ALTER TABLE ONLY atsetam_part SET ACCESS METHOD ao_row WITH (blocksize=65536, compresslevel=7);

CREATE TABLE atsetam_part_3 partition OF atsetam_part FOR VALUES FROM (200) to (300);
CREATE TABLE atsetam_part_4 partition OF atsetam_part FOR VALUES FROM (300) to (400) PARTITION BY RANGE(a);
CREATE TABLE atsetam_part_4_1 partition OF atsetam_part_4 FOR VALUES FROM (300) to (350);

SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'atsetam_part%';

DROP TABLE atsetam_part;

-- Altering the storage model at a leaf should change the storage model for the leaf.
CREATE TABLE atsetam_part(a int, b int) PARTITION BY RANGE(a);
CREATE TABLE atsetam_part_1 partition OF atsetam_part FOR VALUES FROM (1) to (100);
CREATE TABLE atsetam_part_2 partition OF atsetam_part FOR VALUES FROM (100) to (200);

ALTER TABLE ONLY atsetam_part_1 SET ACCESS METHOD ao_row WITH (blocksize=65536, compresslevel=7);

SELECT c.relname, a.amname, c.reloptions FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'atsetam_part%';

DROP TABLE atsetam_part;

-- Final scenario: the iterations of altering table from storage type "A" to "B" and back to "A".
-- The following cases will cover all variations of such iterations:
-- 1. Heap->AO->Heap->AO
-- 2. AO->AOCO->AO->AOCO
-- 3. Heap->AOCO->Heap->AOCO

-- create helper function
CREATE FUNCTION check_atsetam(p_tablename text, p_am text, p_reloptions text, p_expect_empty_reloptions bool, p_expect_relfile_change bool)
RETURNS boolean
LANGUAGE plpgsql AS $$
DECLARE
    v_result boolean;
    v_ddl text;
BEGIN
    IF p_reloptions = '' THEN
      v_ddl := 'ALTER TABLE ' || p_tablename || ' SET ACCESS METHOD ' || p_am;
    ELSE
      v_ddl := 'ALTER TABLE ' || p_tablename || ' SET ACCESS METHOD ' || p_am || ' WITH (' || p_reloptions || ')';
    END IF;
    CREATE TEMP TABLE before_ddl(segid int, relfilenode oid);
    CREATE TEMP TABLE after_ddl(segid int, relfilenode oid);

    INSERT INTO before_ddl SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname = p_tablename ORDER BY segid;

    EXECUTE v_ddl;

    INSERT INTO after_ddl SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname = p_tablename ORDER BY segid;

    -- check table rewrite
    IF p_expect_relfile_change THEN
      v_result := (SELECT count(a.*)=0 FROM before_ddl b INNER JOIN after_ddl a ON b.segid = a.segid AND b.relfilenode = a.relfilenode);
    ELSE
      v_result := (SELECT count(a.*)=0 FROM before_ddl b, after_ddl a WHERE b.segid = a.segid AND b.relfilenode != a.relfilenode);
    END IF;

    -- check AM
    v_result := v_result AND (SELECT count(c.*)=1 FROM pg_class c, pg_am a WHERE c.relname = p_tablename AND c.relam = a.oid AND a.amname = p_am);

    -- check reloptions. If not expecting NULL, then just check if what we just set is there.
    IF p_expect_empty_reloptions THEN
      v_result := v_result AND (SELECT reloptions IS NULL FROM pg_class WHERE relname = p_tablename);
    ELSIF p_reloptions != '' THEN
      v_result := v_result AND (SELECT count(*)=1 FROM pg_class WHERE relname = p_tablename AND p_reloptions = ANY(reloptions));
    END IF;

    DROP TABLE before_ddl;
    DROP TABLE after_ddl;

    RETURN v_result;
END;
$$;

-- 1. Heap->AO->Heap->AO
CREATE TABLE heapao(a int, b int);
CREATE INDEX heapaoindex ON heapao(b);
INSERT INTO heapao SELECT i,i FROM generate_series(1,100) i;
-- check partitioned table and one child too
CREATE TABLE heapao_part(a int, b int) PARTITION BY RANGE (b);
CREATE TABLE heapao_part_p1 PARTITION OF heapao_part FOR VALUES FROM (0) TO (200);
INSERT INTO heapao_part SELECT i,i FROM generate_series(1,100) i;

SELECT check_atsetam('heapao', 'ao_row', 'compresslevel=3', false, true);
SELECT check_atsetam('heapao', 'heap', '', true, true);
SELECT check_atsetam('heapao', 'ao_row', '', false, true);
SELECT check_atsetam('heapao_part', 'ao_row', 'compresslevel=3', false, false);
SELECT check_atsetam('heapao_part', 'heap', '', true, false);
SELECT check_atsetam('heapao_part_p1', 'ao_row', 'compresslevel=3', false, true);
SELECT check_atsetam('heapao_part_p1', 'heap', 'fillfactor=70', false, true);
SELECT check_atsetam('heapao_part_p1', 'ao_row', '', false, true);

-- Just checking data is intact
SELECT sum(a)+sum(b) FROM heapao;
SELECT sum(a)+sum(b) FROM heapao_part;
DROP TABLE heapao;
DROP TABLE heapao_part;

-- 2. AO->AOCO->AO->AOCO
CREATE TABLE aoco(a int, b int) with (appendoptimized=true);
CREATE INDEX aocoindex ON aoco(b);
INSERT INTO aoco SELECT i,i FROM generate_series(1,100) i;
-- check partitioned table and one child too
CREATE TABLE aoco_part(a int, b int) PARTITION BY RANGE (b) with (appendoptimized=true);
CREATE TABLE aoco_part_p1 PARTITION OF aoco_part FOR VALUES FROM (0) TO (200);
INSERT INTO aoco_part SELECT i,i FROM generate_series(1,100) i;

SELECT check_atsetam('aoco', 'ao_column', 'compresslevel=5', false, true);
SELECT check_atsetam('aoco', 'ao_row', 'compresslevel=7', false, true);
SELECT check_atsetam('aoco', 'ao_column', '', false, true);
SELECT check_atsetam('aoco_part', 'ao_column', 'compresslevel=5', false, false);
SELECT check_atsetam('aoco_part', 'ao_row', '', false, false);
SELECT check_atsetam('aoco_part_p1', 'ao_column', 'compresslevel=3', false, true);
SELECT check_atsetam('aoco_part_p1', 'ao_row', 'compresslevel=7', false, true);
SELECT check_atsetam('aoco_part_p1', 'ao_column', '', false, true);

-- Just checking data is intact
SELECT sum(a)+sum(b) FROM aoco;
SELECT sum(a)+sum(b) FROM aoco_part;
DROP TABLE aoco;
DROP TABLE aoco_part;

-- 3. Heap->AOCO->Heap->AOCO
CREATE TABLE heapco(a int, b int);
CREATE INDEX heapcoindex ON heapco(b);
INSERT INTO heapco SELECT i,i FROM generate_series(1,100) i;
-- check partitioned table and one child too
CREATE TABLE heapco_part(a int, b int) PARTITION BY RANGE (b);
CREATE TABLE heapco_part_p1 PARTITION OF heapco_part FOR VALUES FROM (0) TO (200);
INSERT INTO heapco_part SELECT i,i FROM generate_series(1,100) i;

SELECT check_atsetam('heapco', 'ao_column', 'compresslevel=3', false, true);
SELECT check_atsetam('heapco', 'heap', '', true, true);
SELECT check_atsetam('heapco', 'ao_row', '', false, true);
SELECT check_atsetam('heapco_part', 'ao_row', 'compresslevel=3', false, false);
SELECT check_atsetam('heapco_part', 'heap', '', true, false);
SELECT check_atsetam('heapco_part_p1', 'ao_row', 'compresslevel=3', false, true);
SELECT check_atsetam('heapco_part_p1', 'heap', 'fillfactor=70', false, true);
SELECT check_atsetam('heapco_part_p1', 'ao_row', '', false, true);

-- Just checking data is intact
SELECT sum(a)+sum(b) FROM heapco;
SELECT sum(a)+sum(b) FROM heapco_part;
DROP TABLE heapco;
DROP TABLE heapco_part;

-- Misc cases
-- 
-- ATSETAM with dropped column:
-- The dropped column should still have an entry in pg_attribute_encoding if it is 
-- an AOCO table, just like pg_attribute.
CREATE TABLE atsetam_dropcol(a int, b int, c int, d int);
INSERT INTO atsetam_dropcol VALUES(1,1,1,1);
SELECT count(*) FROM pg_attribute WHERE attrelid = 'atsetam_dropcol'::regclass AND attnum > 0;
SELECT count(*) FROM pg_attribute_encoding WHERE attrelid = 'atsetam_dropcol'::regclass;
ALTER TABLE atsetam_dropcol DROP COLUMN b;
ALTER TABLE atsetam_dropcol SET ACCESS METHOD ao_row;
SELECT count(*) FROM pg_attribute WHERE attrelid = 'atsetam_dropcol'::regclass AND attnum > 0;
SELECT count(*) FROM pg_attribute_encoding WHERE attrelid = 'atsetam_dropcol'::regclass;
SELECT * FROM atsetam_dropcol;
ALTER TABLE atsetam_dropcol DROP COLUMN c;
ALTER TABLE atsetam_dropcol SET ACCESS METHOD ao_column;
SELECT count(*) FROM pg_attribute WHERE attrelid = 'atsetam_dropcol'::regclass AND attnum > 0;
SELECT count(*) FROM pg_attribute_encoding WHERE attrelid = 'atsetam_dropcol'::regclass;
SELECT * FROM atsetam_dropcol;
ALTER TABLE atsetam_dropcol DROP COLUMN d;
ALTER TABLE atsetam_dropcol SET ACCESS METHOD heap;
SELECT count(*) FROM pg_attribute WHERE attrelid = 'atsetam_dropcol'::regclass AND attnum > 0;
SELECT count(*) FROM pg_attribute_encoding WHERE attrelid = 'atsetam_dropcol'::regclass;
SELECT * FROM atsetam_dropcol;
