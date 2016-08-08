-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- HEAP View
--
CREATE VIEW resync_heap_view1 AS SELECT *  from resync_heap_table;
SELECT count(*) FROM resync_heap_view1;
CREATE VIEW resync_heap_view2 AS SELECT *  from resync_heap_table;
SELECT count(*) FROM resync_heap_view2;
CREATE VIEW resync_heap_view3 AS SELECT *  from resync_heap_table;
SELECT count(*) FROM resync_heap_view3;
--
--
-- AO View
--
--
CREATE VIEW resync_ao_view1 AS SELECT *  from resync_ao_table;
SELECT count(*) FROM resync_ao_view1;
CREATE VIEW resync_ao_view2 AS SELECT *  from resync_ao_table;
SELECT count(*) FROM resync_ao_view2;
CREATE VIEW resync_ao_view3 AS SELECT *  from resync_ao_table;
SELECT count(*) FROM resync_ao_view3;
--
--
-- CO View
--
--
CREATE VIEW resync_co_view1 AS SELECT *  from resync_co_table;
SELECT count(*) FROM resync_co_view1;
CREATE VIEW resync_co_view2 AS SELECT *  from resync_co_table;
SELECT count(*) FROM resync_co_view2;
CREATE VIEW resync_co_view3 AS SELECT *  from resync_co_table;
SELECT count(*) FROM resync_co_view3;
--
--
-- DROP View
--
--
DROP VIEW sync1_heap_view6;
DROP VIEW ck_sync1_heap_view5;
DROP VIEW ct_heap_view3;
DROP VIEW resync_heap_view1;
--
DROP VIEW sync1_ao_view6;
DROP VIEW ck_sync1_ao_view5;
DROP VIEW ct_ao_view3;
DROP VIEW resync_ao_view1;
--
DROP VIEW sync1_co_view6;
DROP VIEW ck_sync1_co_view5;
DROP VIEW ct_co_view3;
DROP VIEW resync_co_view1;
--
