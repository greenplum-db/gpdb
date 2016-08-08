-- start_ignore
-- ST_Area
-- ST_AsBinary
-- ST_AsEWKT
-- ST_AsGML
-- ST_AsGeoJSON
-- ST_AsKML
-- ST_AsSVG
-- ST_AsText
-- ST_Azimuth
-- ST_Buffer
-- ST_CoveredBy
-- ST_Covers
-- ST_DWithin
-- ST_Distance
-- ST_GeogFromText
-- ST_GeogFromWKB
-- ST_GeographyFromText
-- =
-- ST_Intersection
-- ST_Intersects
-- ST_Length
-- ST_Perimeter
-- ST_Project
-- ST_Summary
-- &&


DROP TABLE IF EXISTS geography_test;
-- end_ignore
CREATE TABLE geography_test (id INT4, name text, geog GEOGRAPHY);

INSERT INTO geography_test(id, name, geog) VALUES (1, 'A1', 'POINT(10 10)');
INSERT INTO geography_test(id, name, geog) VALUES (11, 'A2', 'POINT(10 30)');
INSERT INTO geography_test(id, name, geog) VALUES (2, 'B', 'POLYGON((1 1, 2 2, 2 4, 4 2, 1 1))');
INSERT INTO geography_test(id, name, geog) VALUES (3, 'C', 'MULTIPOLYGON(((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 2,1 1)), ((-1 -1,-1 -2,-2 -2,-2 -1,-1 -1)))');
INSERT INTO geography_test(id, name, geog) VALUES (4, 'D', 'MULTILINESTRING((0 0,1 1,1 2),(2 3,3 2,5 4))');
INSERT INTO geography_test(id, name, geog) VALUES (5, 'A1', 'LINESTRINg(1 1, 2 2, 2 3, 3 3)');

SELECT GeometryType(geog) FROM geography_test ORDER BY id;

SELECT id, name, ST_AsGML(geog) FROM geography_test;
SELECT id, name, ST_AsText(geog), ST_AsBinary(geog), ST_AsEWKT(geog), ST_AsGeoJSON(geog), ST_AsKML(geog), ST_AsSVG(geog)  FROM geography_test ORDER BY id;

SELECT ST_Azimuth(ST_GeogFromText('POINT(10 10)'), ST_GeogFromText('POINT(10 30)')) FROM geography_test ORDER BY id;
SELECT ST_Azimuth(ST_GeogFromText('POINT(10 10)'), ST_GeogFromText('POINT(30 10)')) FROM geography_test ORDER BY id;

SELECT ST_AsText(ST_SnapToGrid(ST_Buffer(ST_GeogFromText(ST_AsText(geog)), 1)::geometry, 0.00001)) FROM geography_test ORDER BY id;

SELECT ST_CoveredBy(geog, ST_GeogFromText('POLYGON((1 1, 5 1, 10 2, 2 5, 1 1))')) FROM geography_test WHERE GeometryType(geog) = 'POINT' ORDER BY id;

SELECT ST_Covers(ST_GeogFromText('POLYGON((1 1, 5 1, 10 2, 2 5, 1 1))'), geog) FROM geography_test WHERE GeometryType(geog) = 'POINT' ORDER BY id;

SELECT ST_DWithin(geog, geog, 1) FROM geography_test ORDER BY id;

SELECT a.id, a.name, b.name FROM geography_test a LEFT JOIN geography_test b ON ST_DWithin(a.geog, b.geog, 100000);

SELECT ST_Distance(geog, ST_GeomFromText('POINT(10 10)', 4326)), ST_Distance(geog, ST_GeomFromText('POINT(0 0)', 4326)) FROM geography_test ORDER BY id;

SELECT ST_GeogFromText(ST_AsText(geog)), ST_GeogFromWKB(ST_AsBinary(geog)), ST_GeographyFromText(ST_AsText(geog)) FROM geography_test ORDER BY id;

SELECT ST_AsText(ST_Intersection(geog, ST_GeogFromText('POLYGON((1 1, 5 1, 10 2, 2 5, 1 1))'))) FROM geography_test ORDER BY id;

SELECT ST_Intersects(geog, ST_GeogFromText('POLYGON((1 1, 5 1, 10 2, 2 5, 1 1))')) FROM geography_test ORDER BY id;

--SELECT ST_Length(ST_GeogFromText('LINESTRING(1 1, 5 1, 10 2, 2 5)')) ORDER BY id;
SELECT id, name, ST_Length(geog) FROM geography_test ORDER BY id;

SELECT ST_AsText(geog), ST_Perimeter(geog::geometry) FROM geography_test ORDER BY id;

SELECT ST_AsText(geog), ST_Summary(geog) FROM geography_test ORDER BY id;

SELECT ST_AsText(geog), geog && ST_GeogFromText('LINESTRING(1 1, 5 1, 10 2, 2 5)') FROM geography_test ORDER BY id;

DROP TABLE geography_test;

