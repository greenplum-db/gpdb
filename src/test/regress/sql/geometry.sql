--
-- GEOMETRY
--

-- create needed tables
CREATE TABLE BOX_GEOM_TBL (f1 box);
INSERT INTO BOX_GEOM_TBL (f1) VALUES ('(2.0,2.0,0.0,0.0)');
INSERT INTO BOX_GEOM_TBL (f1) VALUES ('(1.0,1.0,3.0,3.0)');
INSERT INTO BOX_GEOM_TBL (f1) VALUES ('(2.5, 2.5, 2.5,3.5)');
INSERT INTO BOX_GEOM_TBL (f1) VALUES ('(3.0, 3.0,3.0,3.0)');

CREATE TABLE CIRCLE_GEOM_TBL (f1 circle);
INSERT INTO CIRCLE_GEOM_TBL VALUES ('<(5,1),3>');
INSERT INTO CIRCLE_GEOM_TBL VALUES ('<(1,2),100>');
INSERT INTO CIRCLE_GEOM_TBL VALUES ('1,3,5');
INSERT INTO CIRCLE_GEOM_TBL VALUES ('((1,2),3)');
INSERT INTO CIRCLE_GEOM_TBL VALUES ('<(100,200),10>');
INSERT INTO CIRCLE_GEOM_TBL VALUES ('<(100,1),115>');

CREATE TABLE LSEG_GEOM_TBL (s lseg);
INSERT INTO LSEG_GEOM_TBL VALUES ('[(1,2),(3,4)]');
INSERT INTO LSEG_GEOM_TBL VALUES ('(0,0),(6,6)');
INSERT INTO LSEG_GEOM_TBL VALUES ('10,-10 ,-3,-4');
INSERT INTO LSEG_GEOM_TBL VALUES ('[-1e6,2e2,3e5, -4e1]');
INSERT INTO LSEG_GEOM_TBL VALUES ('(11,22,33,44)');

CREATE TABLE PATH_GEOM_TBL (s serial, f1 path);
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('[(1,2),(3,4)]');
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('((1,2),(3,4))');
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('[(0,0),(3,0),(4,5),(1,6)]');
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('((1,2),(3,4))');
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('1,2 ,3,4');
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('[1,2,3, 4]');
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('[11,12,13,14]');
INSERT INTO PATH_GEOM_TBL(f1) VALUES ('(11,12,13,14)');

CREATE TABLE POLYGON_GEOM_TBL(s serial, f1 polygon);
INSERT INTO POLYGON_GEOM_TBL(f1) VALUES ('(2.0,0.0),(2.0,4.0),(0.0,0.0)');
INSERT INTO POLYGON_GEOM_TBL(f1) VALUES ('(3.0,1.0),(3.0,3.0),(1.0,0.0)');
INSERT INTO POLYGON_GEOM_TBL(f1) VALUES ('(0.0,0.0)');
INSERT INTO POLYGON_GEOM_TBL(f1) VALUES ('(0.0,1.0),(0.0,1.0)');

CREATE TABLE POINT_GEOM_TBL(f1 point);
INSERT INTO POINT_GEOM_TBL(f1) VALUES ('(0.0,0.0)');
INSERT INTO POINT_GEOM_TBL(f1) VALUES ('(-10.0,0.0)');
INSERT INTO POINT_GEOM_TBL(f1) VALUES ('(-3.0,4.0)');
INSERT INTO POINT_GEOM_TBL(f1) VALUES ('(5.1, 34.5)');
INSERT INTO POINT_GEOM_TBL(f1) VALUES ('(-5.0,-12.0)');
INSERT INTO POINT_GEOM_TBL(f1) VALUES ('10.0,10.0');

-- Back off displayed precision a little bit to reduce platform-to-platform
-- variation in results.
SET extra_float_digits TO -3;

--
-- Points
--

SELECT '' AS four, center(f1) AS center
   FROM BOX_GEOM_TBL ORDER BY (f1[0])[0],(f1[1])[0];

SELECT '' AS four, (@@ f1) AS center
   FROM BOX_GEOM_TBL ORDER BY (f1[0])[0],(f1[1])[0];

SELECT '' AS six, point(f1) AS center
   FROM CIRCLE_GEOM_TBL ORDER BY (center(f1))[0],diameter(f1);

SELECT '' AS six, (@@ f1) AS center
   FROM CIRCLE_GEOM_TBL ORDER BY (center(f1))[0],diameter(f1);

SELECT '' AS two, (@@ f1) AS center
   FROM POLYGON_GEOM_TBL
   WHERE (# f1) > 2 ORDER BY s;

-- "is horizontal" function
SELECT '' AS two, p1.f1
   FROM POINT_GEOM_TBL p1
   WHERE ishorizontal(p1.f1, point '(0,0)') ORDER BY p1.f1[0],p1.f1[1];

-- "is horizontal" operator
SELECT '' AS two, p1.f1
   FROM POINT_GEOM_TBL p1
   WHERE p1.f1 ?- point '(0,0)' ORDER BY p1.f1[0],p1.f1[1];

-- "is vertical" function
SELECT '' AS one, p1.f1
   FROM POINT_GEOM_TBL p1
   WHERE isvertical(p1.f1, point '(5.1,34.5)') ORDER BY p1.f1[0],p1.f1[1];

-- "is vertical" operator
SELECT '' AS one, p1.f1
   FROM POINT_GEOM_TBL p1
   WHERE p1.f1 ?| point '(5.1,34.5)' ORDER BY p1.f1[0],p1.f1[1];

--
-- Line segments
--

-- intersection
SELECT '' AS count, p.f1, l.s, l.s # p.f1 AS intersection
   FROM LSEG_GEOM_TBL l, POINT_GEOM_TBL p;

-- closest point
SELECT '' AS thirty, p.f1, l.s, p.f1 ## l.s AS closest
   FROM LSEG_GEOM_TBL l, POINT_GEOM_TBL p ORDER BY p.f1[0],p.f1[1],(s[0])[0];

--
-- Lines
--

--
-- Boxes
--

SELECT '' as six, box(f1) AS box FROM CIRCLE_GEOM_TBL ORDER BY (center(f1))[0],diameter(f1);

-- translation
SELECT '' AS twentyfour, b.f1 + p.f1 AS translation
   FROM BOX_GEOM_TBL b, POINT_GEOM_TBL p ORDER BY p.f1[0],p.f1[1], (b.f1[0])[0],(b.f1[1])[0];

SELECT '' AS twentyfour, b.f1 - p.f1 AS translation
   FROM BOX_GEOM_TBL b, POINT_GEOM_TBL p ORDER BY p.f1[0],p.f1[1], (b.f1[0])[0],(b.f1[1])[0];

-- scaling and rotation
SELECT '' AS twentyfour, b.f1 * p.f1 AS rotation
   FROM BOX_GEOM_TBL b, POINT_GEOM_TBL p ORDER BY p.f1[0],p.f1[1], (b.f1[0])[0],(b.f1[1])[0];

SELECT '' AS twenty, b.f1 / p.f1 AS rotation
   FROM BOX_GEOM_TBL b, POINT_GEOM_TBL p
   WHERE (p.f1 <-> point '(0,0)') >= 1 ORDER BY p.f1[0],p.f1[1], (b.f1[0])[0],(b.f1[1])[0];

--
-- Paths
--

SELECT '' AS eight, npoints(f1) AS npoints, f1 AS path FROM PATH_GEOM_TBL ORDER BY s;

SELECT '' AS four, path(f1) FROM POLYGON_GEOM_TBL ORDER BY s;

-- translation
SELECT '' AS eight, p1.f1 + point '(10,10)' AS dist_add
   FROM PATH_GEOM_TBL p1 ORDER BY s;

-- scaling and rotation
SELECT '' AS eight, p1.f1 * point '(2,-1)' AS dist_mul
   FROM PATH_GEOM_TBL p1 ORDER BY s;

--
-- Polygons
--

-- containment
SELECT '' AS twentyfour, p.f1, poly.f1, poly.f1 @> p.f1 AS contains
   FROM POLYGON_GEOM_TBL poly, POINT_GEOM_TBL p ORDER BY p.f1[0],p.f1[1],poly.s;

SELECT '' AS twentyfour, p.f1, poly.f1, p.f1 <@ poly.f1 AS contained
   FROM POLYGON_GEOM_TBL poly, POINT_GEOM_TBL p ORDER BY  p.f1[0],p.f1[1],poly.s;

SELECT '' AS four, npoints(f1) AS npoints, f1 AS polygon
   FROM POLYGON_GEOM_TBL ORDER BY s;

SELECT '' AS four, polygon(f1)
   FROM BOX_GEOM_TBL ORDER BY (f1[0])[0],(f1[1])[0];

SELECT '' AS four, polygon(f1)
   FROM PATH_GEOM_TBL WHERE isclosed(f1) ORDER BY s;

SELECT '' AS four, f1 AS open_path, polygon( pclose(f1)) AS polygon
   FROM PATH_GEOM_TBL
   WHERE isopen(f1) ORDER BY s;

-- convert circles to polygons using the default number of points
SELECT '' AS six, polygon(f1)
   FROM CIRCLE_GEOM_TBL ORDER BY (center(f1))[0],diameter(f1);

-- convert the circle to an 8-point polygon
SELECT '' AS six, polygon(8, f1)
   FROM CIRCLE_GEOM_TBL ORDER BY (center(f1))[0],diameter(f1);

--
-- Circles
--

SELECT '' AS six, circle(f1, 50.0)
   FROM POINT_GEOM_TBL ORDER BY f1[0],f1[1];

SELECT '' AS four, circle(f1)
   FROM BOX_GEOM_TBL ORDER BY (f1[0])[0],(f1[1])[0];

SELECT '' AS two, circle(f1)
   FROM POLYGON_GEOM_TBL
   WHERE (# f1) >= 3 ORDER BY s;

SELECT '' AS twentyfour, c1.f1 AS circle, p1.f1 AS point, (p1.f1 <-> c1.f1) AS distance
   FROM CIRCLE_GEOM_TBL c1, POINT_GEOM_TBL p1
   WHERE (p1.f1 <-> c1.f1) > 0
   ORDER BY distance, area(c1.f1), p1.f1[0];

-- Clean up GPDB-added tables
DROP TABLE box_geom_tbl;
DROP TABLE circle_geom_tbl;
DROP TABLE lseg_geom_tbl;
DROP TABLE path_geom_tbl;
DROP TABLE polygon_geom_tbl;
DROP TABLE point_geom_tbl;
