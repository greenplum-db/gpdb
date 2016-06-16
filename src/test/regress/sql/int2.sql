--
-- INT2
-- NOTE: int2 operators never check for over/underflow!
-- Some of these answers are consequently numerically incorrect.
--

CREATE TABLE INT2_TBL(f1 int2);

INSERT INTO INT2_TBL(f1) VALUES ('0   ');

INSERT INTO INT2_TBL(f1) VALUES ('  1234 ');

INSERT INTO INT2_TBL(f1) VALUES ('    -1234');

INSERT INTO INT2_TBL(f1) VALUES ('34.5');

-- largest and smallest values
INSERT INTO INT2_TBL(f1) VALUES ('32767');

INSERT INTO INT2_TBL(f1) VALUES ('-32767');

-- bad input values -- should give errors
INSERT INTO INT2_TBL(f1) VALUES ('100000');
INSERT INTO INT2_TBL(f1) VALUES ('asdf');
INSERT INTO INT2_TBL(f1) VALUES ('    ');
INSERT INTO INT2_TBL(f1) VALUES ('- 1234');
INSERT INTO INT2_TBL(f1) VALUES ('4 444');
INSERT INTO INT2_TBL(f1) VALUES ('123 dt');
INSERT INTO INT2_TBL(f1) VALUES ('');


SELECT '' AS five, * FROM INT2_TBL order by f1;

SELECT '' AS four, i.* FROM INT2_TBL i WHERE i.f1 <> int2 '0' order by f1;

SELECT '' AS four, i.* FROM INT2_TBL i WHERE i.f1 <> int4 '0' order by f1;

SELECT '' AS one, i.* FROM INT2_TBL i WHERE i.f1 = int2 '0' order by f1;

SELECT '' AS one, i.* FROM INT2_TBL i WHERE i.f1 = int4 '0' order by f1;

SELECT '' AS two, i.* FROM INT2_TBL i WHERE i.f1 < int2 '0' order by f1;

SELECT '' AS two, i.* FROM INT2_TBL i WHERE i.f1 < int4 '0' order by f1;

SELECT '' AS three, i.* FROM INT2_TBL i WHERE i.f1 <= int2 '0' order by f1;

SELECT '' AS three, i.* FROM INT2_TBL i WHERE i.f1 <= int4 '0' order by f1;

SELECT '' AS two, i.* FROM INT2_TBL i WHERE i.f1 > int2 '0' order by f1;

SELECT '' AS two, i.* FROM INT2_TBL i WHERE i.f1 > int4 '0' order by f1;

SELECT '' AS three, i.* FROM INT2_TBL i WHERE i.f1 >= int2 '0' order by f1;

SELECT '' AS three, i.* FROM INT2_TBL i WHERE i.f1 >= int4 '0' order by f1;

-- positive odds 
SELECT '' AS one, i.* FROM INT2_TBL i WHERE (i.f1 % int2 '2') = int2 '1' order by f1;

-- any evens 
SELECT '' AS three, i.* FROM INT2_TBL i WHERE (i.f1 % int4 '2') = int2 '0' order by f1;

SELECT '' AS five, i.f1, i.f1 * int2 '2' AS x FROM INT2_TBL i order by f1;

SELECT '' AS five, i.f1, i.f1 * int2 '2' AS x FROM INT2_TBL i
WHERE abs(f1) < 16384 order by f1;

SELECT '' AS five, i.f1, i.f1 * int4 '2' AS x FROM INT2_TBL i order by f1;

SELECT '' AS five, i.f1, i.f1 + int2 '2' AS x FROM INT2_TBL i order by f1;

SELECT '' AS five, i.f1, i.f1 + int2 '2' AS x FROM INT2_TBL i
WHERE f1 < 32766 order by f1;

SELECT '' AS five, i.f1, i.f1 + int4 '2' AS x FROM INT2_TBL i order by f1;

SELECT '' AS five, i.f1, i.f1 - int2 '2' AS x FROM INT2_TBL i order by f1;

SELECT '' AS five, i.f1, i.f1 - int2 '2' AS x FROM INT2_TBL i
WHERE f1 > -32767 order by f1;

SELECT '' AS five, i.f1, i.f1 - int4 '2' AS x FROM INT2_TBL i order by f1;

SELECT '' AS five, i.f1, i.f1 / int2 '2' AS x FROM INT2_TBL i order by f1;

SELECT '' AS five, i.f1, i.f1 / int4 '2' AS x FROM INT2_TBL i order by f1;

-- check sane handling of INT16_MIN overflow cases
SELECT (-32768)::int2 * (-1)::int2;
SELECT (-32768)::int2 / (-1)::int2;
SELECT (-32768)::int2 % (-1)::int2;
