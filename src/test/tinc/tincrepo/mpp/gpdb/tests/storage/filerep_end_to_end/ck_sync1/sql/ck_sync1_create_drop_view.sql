-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- HEAP View
--
CREATE VIEW ck_sync1_heap_view1 AS SELECT *  from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_heap_view1;
CREATE VIEW ck_sync1_heap_view2 AS SELECT *  from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_heap_view2;
CREATE VIEW ck_sync1_heap_view3 AS SELECT *  from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_heap_view3;
CREATE VIEW ck_sync1_heap_view4 AS SELECT *  from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_heap_view4;
CREATE VIEW ck_sync1_heap_view5 AS SELECT *  from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_heap_view5;
CREATE VIEW ck_sync1_heap_view6 AS SELECT *  from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_heap_view6;
CREATE VIEW ck_sync1_heap_view7 AS SELECT *  from ck_sync1_heap_table;
SELECT count(*) FROM ck_sync1_heap_view7;
--
--
-- AO View
--
--
CREATE VIEW ck_sync1_ao_view1 AS SELECT *  from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_ao_view1;
CREATE VIEW ck_sync1_ao_view2 AS SELECT *  from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_ao_view2;
CREATE VIEW ck_sync1_ao_view3 AS SELECT *  from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_ao_view3;
CREATE VIEW ck_sync1_ao_view4 AS SELECT *  from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_ao_view4;
CREATE VIEW ck_sync1_ao_view5 AS SELECT *  from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_ao_view5;
CREATE VIEW ck_sync1_ao_view6 AS SELECT *  from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_ao_view6;
CREATE VIEW ck_sync1_ao_view7 AS SELECT *  from ck_sync1_ao_table;
SELECT count(*) FROM ck_sync1_ao_view7;
--
--
-- CO View
--
--
CREATE VIEW ck_sync1_co_view1 AS SELECT *  from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_co_view1;
CREATE VIEW ck_sync1_co_view2 AS SELECT *  from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_co_view1;
CREATE VIEW ck_sync1_co_view3 AS SELECT *  from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_co_view1;
CREATE VIEW ck_sync1_co_view4 AS SELECT *  from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_co_view1;
CREATE VIEW ck_sync1_co_view5 AS SELECT *  from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_co_view1;
CREATE VIEW ck_sync1_co_view6 AS SELECT *  from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_co_view1;
CREATE VIEW ck_sync1_co_view7 AS SELECT *  from ck_sync1_co_table;
SELECT count(*) FROM ck_sync1_co_view1;
--
--
-- DROP View
--
--
DROP VIEW sync1_heap_view2;
DROP VIEW ck_sync1_heap_view1;
--
DROP VIEW sync1_ao_view2;
DROP VIEW ck_sync1_ao_view1;
--
DROP VIEW sync1_co_view2;
DROP VIEW ck_sync1_co_view1;
--
