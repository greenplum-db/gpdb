--
-- Test union bitmap batch words for multivalues index scan like where x in (x1, x2) or x > v
-- which creates BitmapAnd plan on two bitmap indexs that match multiple keys by using in in where clause
--
CREATE TABLE bmunion (a int, b int);
INSERT INTO bmunion
  SELECT (r%53), (r%59)
  FROM generate_series(1,70000) r;
CREATE INDEX i_bmtest2_a ON bmunion USING BITMAP(a);
CREATE INDEX i_bmtest2_b ON bmunion USING BITMAP(b);
INSERT INTO bmunion SELECT 53, 1 FROM generate_series(1, 1000);

SET optimizer_enable_tablescan=OFF;
SET optimizer_enable_dynamictablescan=OFF;
-- Inject fault for planner so that it could produce bitMapAnd plan node.
SELECT gp_inject_fault('simulate_bitmap_and', 'skip', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = -1;
EXPLAIN (COSTS OFF) SELECT count(*) FROM bmunion WHERE a = 53 AND b < 3;
SELECT gp_inject_fault('simulate_bitmap_and', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

SELECT gp_inject_fault('simulate_bitmap_and', 'skip', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = -1;
SELECT count(*) FROM bmunion WHERE a = 53 AND b < 3;
SELECT gp_inject_fault('simulate_bitmap_and', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

RESET optimizer_enable_tablescan;
RESET optimizer_enable_dynamictablescan;

DROP TABLE bmunion;
