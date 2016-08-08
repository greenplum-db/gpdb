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

-- start_ignore
DROP TABLE IF EXISTS roads;
-- end_ignore
CREATE TABLE roads (id INT4, name text);
SELECT AddGeometryColumn('public','roads','geom',0,'GEOMETRY',2);
INSERT INTO roads(id,geom,name) VALUES (1, ST_GeomFromText('LINESTRING(1 1.5 ,4 1.5)',-1),'1st St');
INSERT INTO roads(id,geom,name) VALUES (2, ST_GeomFromText('LINESTRING(3 1 ,3 8, 2 8)',-1),'2nd St');
INSERT INTO roads(id,geom,name) VALUES (3, ST_GeomFromText('LINESTRING(1 2 ,4 4)',-1),'3rd St');
INSERT INTO roads(id,geom,name) VALUES (4, ST_GeomFromText('LINESTRING(2.5 7 ,2.5 8)',-1),'4th St');
INSERT INTO roads(id,geom,name) VALUES (5, ST_GeomFromText('LINESTRING(7 4 ,8 4)',-1),'5th St');

--What is total length of road?
SELECT sum(ST_Length(geom)) as Total_length FROM roads;

--Return a point halfway between each road
SELECT name, ST_ASTEXT(ST_line_interpolate_point(geom,0.5)) as half FROM roads ORDER BY half;

--What is the total length of road fully contained in each region?
SELECT m.name, sum(ST_length(r.geom)) as road_length
FROM roads as r, areas as m
WHERE ST_contains(m.geom, r.geom)
GROUP BY m.name
ORDER BY road_length, name;

--Filter all roads contained within region A in new table
-- start_ignore
DROP TABLE IF EXISTS roadsA;
-- end_ignore
CREATE TABLE roadsA as 
SELECT ST_ASText(ST_Intersection(r.geom, m.geom)) as intersection, ST_length(r.geom) as orig_length, r.name
FROM roads as r, areas as m
WHERE m.name = 'A' AND ST_intersects(r.geom, m.geom)
ORDER BY orig_length;

SELECT * FROM roadsA ORDER BY orig_length;

DROP TABLE areas;
DROP TABLE roads;
DROP TABLE roadsA;

