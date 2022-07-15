-- Check changing table access method

-- Scenario 1: Heap to Heap
CREATE TABLE heap2heap(a int, b int) DISTRIBUTED BY (a);
CREATE TABLE heap2heap2(a int, b int) DISTRIBUTED BY (a);

INSERT INTO heap2heap SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heap2heap2 SELECT i,i FROM generate_series(1,5) i;

CREATE TEMP TABLE relfilebeforeheap AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heap2heap', 'heap2heap2')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heap2heap', 'heap2heap2') ORDER BY segid;

-- changing to the same access method shouldn't rewrite the table
-- (i.e. the relfilenodes shouldn't change)
ALTER TABLE heap2heap SET ACCESS METHOD heap;
ALTER TABLE heap2heap2 SET WITH (appendoptimized=false);

CREATE TEMP TABLE relfileafterheap AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heap2heap', 'heap2heap2')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heap2heap', 'heap2heap2') ORDER BY segid;

-- relfilenodes shouldn't change
SELECT count(*) FROM (SELECT * FROM relfilebeforeheap UNION SELECT * FROM relfileafterheap)a;


-- Scenario 2: Heap to AO
CREATE TABLE heap2ao(a int, b int) WITH (fillfactor=70) DISTRIBUTED BY (a);
CREATE TABLE heap2ao2(a int, b int) DISTRIBUTED BY (a);

CREATE INDEX heapi ON heap2ao(b);
ALTER TABLE heap2ao2 ADD CONSTRAINT unique_constraint UNIQUE (a);

INSERT INTO heap2ao SELECT i,i FROM generate_series(1,5) i;
INSERT INTO heap2ao2 SELECT i,i FROM generate_series(1,5) i;

CREATE TEMP TABLE relfilebeforeao AS
    SELECT -1 segid, relfilenode FROM pg_class WHERE relname in ('heap2ao', 'heap2ao2', 'heapi')
    UNION SELECT gp_segment_id segid, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname in ('heap2ao', 'heap2ao2', 'heapi') ORDER BY segid;

-- Altering a heap table with a unique index to AO should error out
-- as unique indexes aren't supported on AO tables
ALTER TABLE heap2ao2 SET ACCESS METHOD ao_row;
ALTER TABLE heap2ao2 DROP CONSTRAINT unique_constraint;

-- Set default storage options for the table to inherit from
SET gp_default_storage_options = 'blocksize=65536, compresstype=zlib, compresslevel=5, checksum=true';

-- Alter table heap to AO should work
ALTER TABLE heap2ao SET ACCESS METHOD ao_row;
ALTER TABLE heap2ao2 SET WITH (appendoptimized=true);

-- The altered tables should have AO AM
SELECT c.relname, a.amname FROM pg_class c JOIN pg_am a ON c.relam = a.oid WHERE c.relname LIKE 'heap2ao%';

-- The altered tables should inherit storage options from gp_default_storage_options
SELECT blocksize,compresslevel,checksum,compresstype,columnstore
FROM pg_appendonly WHERE relid in ('heap2ao'::regclass::oid, 'heap2ao2'::regclass::oid);
SELECT reloptions from pg_class where relname in ('heap2ao', 'heap2ao2');

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
-- FIXME: add check for gp_aoblkdir
SELECT * FROM gp_toolkit.__gp_aoseg('heap2ao');
SELECT * FROM gp_toolkit.__gp_aovisimap('heap2ao');
SELECT * FROM gp_toolkit.__gp_aoseg('heap2ao2');
SELECT * FROM gp_toolkit.__gp_aovisimap('heap2ao2');

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
SELECT blocksize,compresslevel,checksum,compresstype,columnstore
FROM pg_appendonly WHERE relid in ('heapbase'::regclass::oid, 'heapbase2'::regclass::oid);
SELECT reloptions from pg_class where relname in ('heapbase','heapbase2');

SELECT blocksize,compresslevel,checksum,compresstype,columnstore
FROM pg_appendonly WHERE relid in ('heapchild'::regclass::oid, 'heapchild2'::regclass::oid);
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
SELECT * FROM gp_toolkit.__gp_aovisimap('heapbase');
SELECT * FROM gp_toolkit.__gp_aoseg('heapbase2');
SELECT * FROM gp_toolkit.__gp_aovisimap('heapbase2');
SELECT c.relname FROM pg_class c, pg_appendonly p WHERE c.relname LIKE 'heapbase%' AND c.oid = p.relid;

-- aux tables are not created for child table
SELECT * FROM gp_toolkit.__gp_aoseg('heapchild');
SELECT * FROM gp_toolkit.__gp_aovisimap('heapchild');
SELECT * FROM gp_toolkit.__gp_aoseg('heapchild2');
SELECT * FROM gp_toolkit.__gp_aovisimap('heapchild2');
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
SELECT * FROM gp_toolkit.__gp_aovisimap('ao2heap');
SELECT * FROM gp_toolkit.__gp_aoseg('ao2heap2');
SELECT * FROM gp_toolkit.__gp_aovisimap('ao2heap2');

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

-- Final scenario: run the iterations of AT from "A" to "B" and back to "A", that includes:
-- 1. Heap->AO->Heap->AO
-- (TODO) 2. AO->AOCO->AO->AOCO
-- (TODO) 3. Heap->AOCO->Heap->AOCO

-- 1. Heap->AO->Heap->AO
CREATE TABLE heapao(a int, b int) WITH (appendonly=true);
CREATE INDEX heapaoindex ON heapao(b);
INSERT INTO heapao SELECT i,i FROM generate_series(1,5) i;

ALTER TABLE heapao SET ACCESS METHOD ao_row;
ALTER TABLE heapao SET ACCESS METHOD heap;
ALTER TABLE heapao SET ACCESS METHOD ao_row;

-- Just checking data is intact. 
SELECT count(*) FROM heapao;
DROP TABLE heapao;
