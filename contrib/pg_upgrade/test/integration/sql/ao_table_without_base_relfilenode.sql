CREATE TABLE ao_without_base_relfilenode (a int ,b int) WITH (appendonly=true);
INSERT INTO ao_without_base_relfilenode VALUES (1,1), (2,2), (3,3);
-- alter the table so that the relfilenod is changed
ALTER TABLE ao_without_base_relfilenode SET DISTRIBUTED RANDOMLY;
-- delete some records so that we have empty base relfilenode files
DELETE FROM ao_without_base_relfilenode;
INSERT INTO ao_without_base_relfilenode VALUES (1,1), (2,2), (3,3);
-- vaccum the table so that the empty base relfilenode files are deleted from system
VACUUM ao_without_base_relfilenode;
-- check that the base relfilenode does not exist on some segments
SELECT pg_stat_file('base/' || db.oid || '/' || pc.relfilenode) from gp_dist_random('pg_class') pc, gp_dist_random('pg_database') db where pc.relname='ao_without_base_relfilenode' and datname = current_database();

CREATE TABLE aoco_without_base_relfilenode WITH (appendonly=true, orientation=column) AS (
  SELECT GENERATE_SERIES::numeric a, GENERATE_SERIES b FROM GENERATE_SERIES(1, 2)
);
UPDATE aoco_without_base_relfilenode SET b=-b;
VACUUM aoco_without_base_relfilenode;
-- check that the base relfilenode does not exist on some segments
SELECT pg_stat_file('base/' || db.oid || '/' || pc.relfilenode) from gp_dist_random('pg_class') pc, gp_dist_random('pg_database') db where pc.relname='aoco_without_base_relfilenode' and datname = current_database();
