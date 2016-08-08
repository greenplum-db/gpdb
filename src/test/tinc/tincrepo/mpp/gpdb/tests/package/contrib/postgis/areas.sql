-- start_ignore
DROP TABLE IF EXISTS areas;
-- end_ignore
CREATE TABLE areas (id INT4, name text);
SELECT AddGeometryColumn('public','areas','geom', 0,'GEOMETRY',2);

INSERT INTO areas(id, geom, name) VALUES (1, ST_GeomFromText('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))'),'A');
INSERT INTO areas(id, geom, name) VALUES (2, ST_GeomFromText('POLYGON((2 7, 2 9, 4 7, 2 7))'),'B');
INSERT INTO areas(id, geom, name) VALUES (3, ST_Translate(ST_GeomFromText('POLYGON((2 7, 2 9, 4 7, 2 7))'),0,-2),'C'); --shift a version of B
--make a donut
INSERT INTO areas(id, geom, name) SELECT 4, ST_BuildArea(ST_Collect(smallc,bigc)),'D' FROM (SELECT
						ST_Buffer( ST_GeomFromText('POINT(6 4)'), 1) As smallc,
						ST_Buffer(ST_GeomFromText('POINT(6 4)'), 2) As bigc) As foo;

--What is the area/perimeter of each region?
SELECT name, st_area(geom), st_perimeter(geom) FROM areas ORDER BY name;
--What type of geometry is this?
SELECT GeometryType(geom) FROM areas;
--Which region is below y=5?
SELECT name FROM areas WHERE areas.geom <<| ST_GeomFromText('LINESTRING(0 5, 10 5)');
--Which region is the triangle one?
SELECT name FROM areas WHERE ST_equals(areas.geom,ST_GeomFromText('POLYGON((4 7, 2 9, 2 7, 4 7))'));
--how many regions are there?
SELECT ST_NumGeometries(ST_UNION(areas.geom)) as NumRegions FROM areas;
--what is the bounding box of the regions?
SELECT ST_Extent(geom) FROM areas;
--how far from origin?
SELECT name, ST_Distance(geom,ST_GeomFromText('POINT(0 0)')) as distance FROM areas ORDER BY distance;

DROP TABLE areas;

