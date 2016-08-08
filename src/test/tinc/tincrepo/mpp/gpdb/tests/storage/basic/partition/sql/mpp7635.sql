--start_ignore
DROP TABLE mpp7635_aoi_table2 CASCADE;
--end_ignore

CREATE TABLE mpp7635_aoi_table2 (id INTEGER)
 PARTITION BY RANGE (id)
  (START (0) END (200000) EVERY (100000))
;
INSERT INTO mpp7635_aoi_table2(id) VALUES (0);
CREATE INDEX mpp7635_ix3 ON mpp7635_aoi_table2 USING BITMAP (id);
select * from pg_indexes where tablename like 'mpp7635%';
DROP INDEX mpp7635_ix3;
select * from pg_indexes where tablename like 'mpp7635%';
CREATE INDEX mpp7635_ix3 ON mpp7635_aoi_table2 (id);
