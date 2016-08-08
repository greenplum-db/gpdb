-- start_ignore
DROP TABLE IF EXISTS management_functions;
-- end_ignore
CREATE TABLE management_functions (id INT4, name text);
SELECT AddGeometryColumn('public','management_functions','geom', 0,'GEOMETRY',2);

INSERT INTO management_functions(id, name, geom) VALUES (1, 'A', ST_GeomFromText('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))'));
INSERT INTO management_functions(id, name, geom) VALUES (2, 'B', ST_GeomFromText('POLYGON((2 7, 2 9, 4 7, 2 7))'));

SELECT GeometryType(geom) FROM management_functions;

SELECT id, name, ST_AsText(geom) FROM management_functions;

SELECT PostGIS_Full_Version();

SELECT Populate_Geometry_Columns();

SELECT UpdateGeometrySRID('public', 'management_functions', 'geom', 0);

SELECT DropGeometryColumn('public', 'management_functions', 'geom');

DROP TABLE management_functions;

