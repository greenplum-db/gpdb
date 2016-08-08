-- start_ignore
DROP TABLE IF EXISTS function3d_test;
-- end_ignore
CREATE TABLE function3d_test (id INT4, name text, geom GEOMETRY);

INSERT INTO function3d_test(id, name, geom) VALUES (1, 'A1', 'POINT(10 10 10)');
INSERT INTO function3d_test(id, name, geom) VALUES (11, 'A2', 'POINT(10 20 30)');
INSERT INTO function3d_test(id, name, geom) VALUES (2, 'B', 'POLYGON((1 1 1, 2 2 2, 2 4 4, 4 2 2, 1 1 1))');
INSERT INTO function3d_test(id, name, geom) VALUES (3, 'C', 'MULTILINESTRING((0 0 0,1 1 1,1 2 2),(2 3 3,3 2 2,5 4 4))');
INSERT INTO function3d_test(id, name, geom) VALUES (4, 'D', 'LINESTRING(1 1 1, 2 2 2, 2 3 3, 3 3 3)');

SELECT GeometryType(geom) FROM function3d_test ORDER BY id;

SELECT id, name, Box3D(geom) FROM function3d_test ORDER BY id;

SELECT id, name, ST_AsX3D(geom) FROM function3d_test ORDER BY id;

SELECT ST_AsText(ST_3DClosestPoint(geom, ST_GeomFromText('POINT(0 0 0)'))) FROM function3d_test ORDER BY id;

SELECT ST_3DLength(geom) FROM function3d_test ORDER BY id;

UPDATE function3d_test SET geom = ST_AddPoint(geom, ST_MakePoint(1, 2, 3)) WHERE GeometryType(geom) = 'LINESTRING';
SELECT id, name, ST_AsText(geom) FROM function3d_test ORDER BY id;

SELECT id, name, ST_AsText(ST_Force_3DZ(geom)) FROM function3d_test ORDER BY id;

DROP TABLE function3d_test;

