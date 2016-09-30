-- Create a simple Append-only table.
-- Insert to it. In two different statements, to get some updates going
-- on the aosegments table associated with it.
CREATE TABLE upgrade_simple_ao(id int4) WITH (appendonly=true);
INSERT INTO upgrade_simple_ao SELECT generate_series(1, 1000);
INSERT INTO upgrade_simple_ao SELECT generate_series(1001, 2000);

-- Also test an append-only table with a numeric column (and some others).
-- The on-disk format of numerics changed between 8.2 and 8.3, make sure
-- that it can be read out correctly after upgrade.
CREATE TABLE upgrade_ao(id int4, n numeric, t text) WITH (appendonly=true);
INSERT INTO upgrade_ao SELECT g, g::numeric / 100, 't' || g FROM generate_series(1, 1000) g;
INSERT INTO upgrade_ao SELECT g, g::numeric / 100, 't' || g FROM generate_series(1001, 2000) g;

-- The same, with a domain over numeric, rather than plain numeric.
CREATE DOMAIN numdomain AS numeric;

-- Also throw in compression, to test that too.
CREATE TABLE upgrade_numdomain_ao (t text, n numdomain) WITH (appendonly=true, compresstype=zlib);
INSERT INTO upgrade_numdomain_ao SELECT 't' || g, g::numeric / 10000 FROM generate_series(1,1000) g;


-- Test upgrade of AO visibility map.
CREATE TABLE upgrade_ao_visimap (distkey int4, id int4) WITH (appendonly=true) DISTRIBUTED BY (distkey) ;
INSERT INTO upgrade_ao_visimap SELECT g, g from generate_series(1, 10000) g;

-- Do some deletions and updates
DELETE FROM upgrade_ao_visimap WHERE id BETWEEN 50 AND 100;
UPDATE upgrade_ao_visimap SET id = -id WHERE id BETWEEN 1000 AND 1050;

-- And rollbacks.
BEGIN;
INSERT INTO upgrade_ao_visimap SELECT g, g from generate_series(100000, 100500) g;
ROLLBACK;
BEGIN;
DELETE FROM upgrade_ao_visimap WHERE id BETWEEN 2000 AND 3000;
ROLLBACK;



-- HEAP TABLE. These are pretty much the same as above, but with heap tables.

-- Also test an append-only table with a numeric column (and some others).
-- The on-disk format of numerics changed between 8.2 and 8.3, make sure
-- that it can be read out correctly after upgrade.
CREATE TABLE upgrade_numeric_heap(id int4, n numeric, t text);
INSERT INTO upgrade_numeric_heap SELECT g, g::numeric / 100, 't' || g FROM generate_series(1, 1000) g;
INSERT INTO upgrade_numeric_heap SELECT g, g::numeric / 100, 't' || g FROM generate_series(1001, 2000) g;

-- The same, with a domain over numeric, rather than plain numeric.
CREATE TABLE upgrade_numdomain_heap (t text, n numdomain);
INSERT INTO upgrade_numdomain_heap SELECT 't' || g, g::numeric / 10000 FROM generate_series(1,1000) g;


-- Test upgrade of AO visibility map.
CREATE TABLE upgrade_heap (distkey int4, id int4) DISTRIBUTED BY (distkey) ;
INSERT INTO upgrade_heap SELECT g, g from generate_series(1, 10000) g;

-- Do some deletions and updates
DELETE FROM upgrade_heap WHERE id BETWEEN 50 AND 100;
UPDATE upgrade_heap SET id = -id WHERE id BETWEEN 1000 AND 1050;

-- And rollbacks.
BEGIN;
INSERT INTO upgrade_heap SELECT g, g from generate_series(100000, 100500) g;
ROLLBACK;
BEGIN;
DELETE FROM upgrade_heap WHERE id BETWEEN 2000 AND 3000;
ROLLBACK;


-- We don't support converting indexes, but make sure the definition gets
-- copied over, and that it gets marked as invalid.
CREATE INDEX i_upgrade_heap ON upgrade_heap(id);


-- HEAP TABLE with full pages.
--
-- The GPDB4 to 5.0 conversion needs to add a field to the page header (pd_prune_xid).
-- That gets significantly more complicated if a page is completely full. Test for that.

CREATE TABLE upgrade_heap_fullpage(distkey int4, t text) WITH (fillfactor = 100) DISTRIBUTED BY (distkey);

INSERT INTO upgrade_heap_fullpage
SELECT 1, CASE g % 800 WHEN 0 THEN repeat('x', (g/800)*2) ELSE lpad(g::text, 6, '0') END
FROM generate_series(1, 80000) g;
