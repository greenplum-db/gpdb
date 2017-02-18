-- @Description Ensures that a vacuum with serializable works ok
-- 
DROP TABLE IF EXISTS ao;
DROP TABLE IF EXISTS ao2;
CREATE TABLE ao (a INT, b INT) WITH (appendonly=true);
CREATE TABLE ao2 (a INT) WITH (appendonly=true);
INSERT INTO ao SELECT i as a, i as b FROM generate_series(1, 100) AS i;

DELETE FROM ao WHERE a < 20;
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT COUNT(*) FROM ao;
SELECT segno, tupcount FROM gp_toolkit.__gp_aoseg_name('ao');
VACUUM ao;
SELECT segno, tupcount FROM gp_toolkit.__gp_aoseg_name('ao');
SELECT COUNT(*) FROM ao;
