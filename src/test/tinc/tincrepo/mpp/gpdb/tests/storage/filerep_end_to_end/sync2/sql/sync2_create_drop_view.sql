-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- HEAP View
--
CREATE VIEW sync2_heap_view1 AS SELECT *  from sync2_heap_table;
SELECT count(*) FROM sync2_heap_view1;
CREATE VIEW sync2_heap_view2 AS SELECT *  from sync2_heap_table;
SELECT count(*) FROM sync2_heap_view2;
--
--
-- AO View
--
--
CREATE VIEW sync2_ao_view1 AS SELECT *  from sync2_ao_table;
SELECT count(*) FROM sync2_ao_view1;
CREATE VIEW sync2_ao_view2 AS SELECT *  from sync2_ao_table;
SELECT count(*) FROM sync2_ao_view2;
--
--
-- CO View
--
--
CREATE VIEW sync2_co_view1 AS SELECT *  from sync2_co_table;
SELECT count(*) FROM sync2_co_view1;
CREATE VIEW sync2_co_view2 AS SELECT *  from sync2_co_table;
SELECT count(*) FROM sync2_co_view2;
--
--
-- DROP View
--
--
DROP VIEW sync1_heap_view7;
DROP VIEW ck_sync1_heap_view6;
DROP VIEW ct_heap_view4;
DROP VIEW resync_heap_view2;
DROP VIEW sync2_heap_view1;
--
DROP VIEW sync1_ao_view7;
DROP VIEW ck_sync1_ao_view6;
DROP VIEW ct_ao_view4;
DROP VIEW resync_ao_view2;
DROP VIEW sync2_ao_view1;
--
DROP VIEW sync1_co_view7;
DROP VIEW ck_sync1_co_view6;
DROP VIEW ct_co_view4;
DROP VIEW resync_co_view2;
DROP VIEW sync2_co_view1;
--
