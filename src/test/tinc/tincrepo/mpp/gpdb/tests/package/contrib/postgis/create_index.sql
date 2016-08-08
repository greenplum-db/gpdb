-- start_ignore
DROP TABLE IF EXISTS create_index;
-- end_ignore
CREATE TABLE create_index (id INT4, name text, geom GEOMETRY);

CREATE INDEX geom_gist ON create_index USING GIST (geom);

INSERT INTO create_index(id, name, geom) VALUES (1, 'A1', 'POINT(10 10 10)');
INSERT INTO create_index(id, name, geom) VALUES (11, 'A2', 'POINT(10 20 30)');
INSERT INTO create_index(id, name, geom) VALUES (2, 'B', 'POLYGON((1 1 1, 2 2 2, 2 4 4, 4 2 2, 1 1 1))');
INSERT INTO create_index(id, name, geom) VALUES (3, 'C', 'MULTILINESTRING((0 0 0,1 1 1,1 2 2),(2 3 3,3 2 2,5 4 4))');
INSERT INTO create_index(id, name, geom) VALUES (4, 'D', 'LINESTRING(1 1 1, 2 2 2, 2 3 3, 3 3 3)');

SELECT GeometryType(geom) FROM create_index ORDER BY id;

SELECT id, name, ST_AsText(geom) FROM create_index WHERE ST_Length(geom) > 0;

SELECT ST_DWithin(geom, geom, 1) FROM create_index ORDER BY id;

DROP TABLE create_index;

