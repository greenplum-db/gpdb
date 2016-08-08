-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- HEAP View
--
CREATE VIEW ct_heap_view1 AS SELECT *  from ct_heap_table;
SELECT count(*) FROM ct_heap_view1;
CREATE VIEW ct_heap_view2 AS SELECT *  from ct_heap_table;
SELECT count(*) FROM ct_heap_view2;
CREATE VIEW ct_heap_view3 AS SELECT *  from ct_heap_table;
SELECT count(*) FROM ct_heap_view3;
CREATE VIEW ct_heap_view4 AS SELECT *  from ct_heap_table;
SELECT count(*) FROM ct_heap_view4;
CREATE VIEW ct_heap_view5 AS SELECT *  from ct_heap_table;
SELECT count(*) FROM ct_heap_view5;
--
--
-- AO View
--
--
CREATE VIEW ct_ao_view1 AS SELECT *  from ct_ao_table;
SELECT count(*) FROM ct_ao_view1;
CREATE VIEW ct_ao_view2 AS SELECT *  from ct_ao_table;
SELECT count(*) FROM ct_ao_view2;
CREATE VIEW ct_ao_view3 AS SELECT *  from ct_ao_table;
SELECT count(*) FROM ct_ao_view3;
CREATE VIEW ct_ao_view4 AS SELECT *  from ct_ao_table;
SELECT count(*) FROM ct_ao_view4;
CREATE VIEW ct_ao_view5 AS SELECT *  from ct_ao_table;
SELECT count(*) FROM ct_ao_view5;
--
--
-- CO View
--
--
CREATE VIEW ct_co_view1 AS SELECT *  from ct_co_table;
SELECT count(*) FROM ct_co_view1;
CREATE VIEW ct_co_view2 AS SELECT *  from ct_co_table;
SELECT count(*) FROM ct_co_view2;
CREATE VIEW ct_co_view3 AS SELECT *  from ct_co_table;
SELECT count(*) FROM ct_co_view3;
CREATE VIEW ct_co_view4 AS SELECT *  from ct_co_table;
SELECT count(*) FROM ct_co_view4;
CREATE VIEW ct_co_view5 AS SELECT *  from ct_co_table;
SELECT count(*) FROM ct_co_view5;
--
--
-- DROP View
--
--
DROP VIEW sync1_heap_view4;
DROP VIEW ck_sync1_heap_view3;
DROP VIEW ct_heap_view1;
--
DROP VIEW sync1_ao_view4;
DROP VIEW ck_sync1_ao_view3;
DROP VIEW ct_ao_view1;
--
DROP VIEW sync1_co_view4;
DROP VIEW ck_sync1_co_view3;
DROP VIEW ct_co_view1;
--
