--
-- INT8
-- Test int8 64-bit integers.
--
CREATE TABLE INT8_TBL(q1 int8, q2 int8);

INSERT INTO INT8_TBL VALUES('  123   ','  456');
INSERT INTO INT8_TBL VALUES('123   ','4567890123456789');
INSERT INTO INT8_TBL VALUES('4567890123456789','123');
INSERT INTO INT8_TBL VALUES('4567890123456789','4567890123456789');
INSERT INTO INT8_TBL VALUES('4567890123456789','-4567890123456789');

-- bad inputs
INSERT INTO INT8_TBL(q1) VALUES ('      ');
INSERT INTO INT8_TBL(q1) VALUES ('xxx');
INSERT INTO INT8_TBL(q1) VALUES ('3908203590239580293850293850329485');
INSERT INTO INT8_TBL(q1) VALUES ('-1204982019841029840928340329840934');
INSERT INTO INT8_TBL(q1) VALUES ('- 123');
INSERT INTO INT8_TBL(q1) VALUES ('  345     5');
INSERT INTO INT8_TBL(q1) VALUES ('');

SELECT * FROM INT8_TBL ;

SELECT '' AS five, q1 AS plus, -q1 AS minus FROM INT8_TBL ;

SELECT '' AS five, q1, q2, q1 + q2 AS plus FROM INT8_TBL ;
SELECT '' AS five, q1, q2, q1 - q2 AS minus FROM INT8_TBL ;
SELECT '' AS three, q1, q2, q1 * q2 AS multiply FROM INT8_TBL ;
SELECT '' AS three, q1, q2, q1 * q2 AS multiply FROM INT8_TBL
 WHERE q1 < 1000 or (q2 > 0 and q2 < 1000) ;
SELECT '' AS five, q1, q2, q1 / q2 AS divide FROM INT8_TBL ;

SELECT '' AS five, q1, float8(q1) FROM INT8_TBL ;
SELECT '' AS five, q2, float8(q2) FROM INT8_TBL ;

SELECT '' AS five, 2 * q1 AS "twice int4" FROM INT8_TBL ;
SELECT '' AS five, q1 * 2 AS "twice int4" FROM INT8_TBL ;

-- TO_CHAR()
--
SELECT '' AS to_char_1, to_char(q1, '9G999G999G999G999G999'), to_char(q2, '9,999,999,999,999,999') 
	FROM INT8_TBL  ;

SELECT '' AS to_char_2, to_char(q1, '9G999G999G999G999G999D999G999'), to_char(q2, '9,999,999,999,999,999.999,999') 
	FROM INT8_TBL  ;

SELECT '' AS to_char_3, to_char( (q1 * -1), '9999999999999999PR'), to_char( (q2 * -1), '9999999999999999.999PR') 
	FROM INT8_TBL  ;

SELECT '' AS to_char_4, to_char( (q1 * -1), '9999999999999999S'), to_char( (q2 * -1), 'S9999999999999999') 
	FROM INT8_TBL  ;

SELECT '' AS to_char_5,  to_char(q2, 'MI9999999999999999')     FROM INT8_TBL  ;
SELECT '' AS to_char_6,  to_char(q2, 'FMS9999999999999999')    FROM INT8_TBL  ;
SELECT '' AS to_char_7,  to_char(q2, 'FM9999999999999999THPR') FROM INT8_TBL ;
SELECT '' AS to_char_8,  to_char(q2, 'SG9999999999999999th')   FROM INT8_TBL ;
SELECT '' AS to_char_9,  to_char(q2, '0999999999999999')       FROM INT8_TBL ;
SELECT '' AS to_char_10, to_char(q2, 'S0999999999999999')      FROM INT8_TBL ;
SELECT '' AS to_char_11, to_char(q2, 'FM0999999999999999')     FROM INT8_TBL ;
SELECT '' AS to_char_12, to_char(q2, 'FM9999999999999999.000') FROM INT8_TBL ;
SELECT '' AS to_char_13, to_char(q2, 'L9999999999999999.000')  FROM INT8_TBL ;
SELECT '' AS to_char_14, to_char(q2, 'FM9999999999999999.999') FROM INT8_TBL ;
SELECT '' AS to_char_15, to_char(q2, 'S 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 . 9 9 9') FROM INT8_TBL ;
SELECT '' AS to_char_16, to_char(q2, E'99999 "text" 9999 "9999" 999 "\\"text between quote marks\\"" 9999') FROM INT8_TBL ;
SELECT '' AS to_char_17, to_char(q2, '999999SG9999999999')     FROM INT8_TBL ;

-- check min/max values
select '-9223372036854775808'::int8;
select '-9223372036854775809'::int8;
select '9223372036854775807'::int8;
select '9223372036854775808'::int8;

-- check sane handling of INT64_MIN overflow cases
SELECT (-9223372036854775808)::int8 * (-1)::int8;
SELECT (-9223372036854775808)::int8 / (-1)::int8;
SELECT (-9223372036854775808)::int8 % (-1)::int8;
SELECT (-9223372036854775808)::int8 * (-1)::int4;
SELECT (-9223372036854775808)::int8 / (-1)::int4;
SELECT (-9223372036854775808)::int8 % (-1)::int4;

-- check behavior when values are NULL
create table int8_null (a int8, b int8, c int8);
insert into int8_null values (1, NULL, NULL);
select a from int8_null where b <=a;
select a from int8_null where b*a <=0;
select a from int8_null where b*c <=0;
select a+b from int8_null;
select b+c from int8_null;
select a-b from int8_null;
select b-c from int8_null;