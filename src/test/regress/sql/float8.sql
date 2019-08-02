--
-- FLOAT8
--

CREATE TABLE FLOAT8_TBL(i INT DEFAULT 1, f1 float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('    0.0   ');
INSERT INTO FLOAT8_TBL(f1) VALUES ('1004.30  ');
INSERT INTO FLOAT8_TBL(f1) VALUES ('   -34.84');
INSERT INTO FLOAT8_TBL(f1) VALUES ('1.2345678901234e+200');
INSERT INTO FLOAT8_TBL(f1) VALUES ('1.2345678901234e-200');

-- test for underflow and overflow handling
SELECT '10e400'::float8;
SELECT '-10e400'::float8;
SELECT '1e309'::float8;
SELECT '10e-400'::float8;
SELECT '-10e-400'::float8;
SELECT '1e-324'::float8;
SELECT '1e308'::float8;
SELECT '1e-323'::float8;
SELECT '0.0'::float8;

-- bad input
INSERT INTO FLOAT8_TBL(f1) VALUES ('');
INSERT INTO FLOAT8_TBL(f1) VALUES ('     ');
INSERT INTO FLOAT8_TBL(f1) VALUES ('xyz');
INSERT INTO FLOAT8_TBL(f1) VALUES ('5.0.0');
INSERT INTO FLOAT8_TBL(f1) VALUES ('5 . 0');
INSERT INTO FLOAT8_TBL(f1) VALUES ('5.   0');
INSERT INTO FLOAT8_TBL(f1) VALUES ('    - 3');
INSERT INTO FLOAT8_TBL(f1) VALUES ('123           5');

-- special inputs
SELECT 'NaN'::float8;
SELECT 'nan'::float8;
SELECT '   NAN  '::float8;
SELECT 'infinity'::float8;
SELECT 'inf'::float8;
SELECT '          -INFINiTY   '::float8;
SELECT '+Infinity'::float8;
SELECT '+INF'::float8;
SELECT '+inf'::float8;
SELECT '+INFINITY'::float8;

-- bad special inputs
SELECT 'N A N'::float8;
SELECT 'NaN x'::float8;
SELECT ' INFINITY    x'::float8;

SELECT 'Infinity'::float8 + 100.0;
SELECT 'Infinity'::float8 / 'Infinity'::float8;
SELECT 'nan'::float8 / 'nan'::float8;
SELECT 'nan'::numeric::float8;

SELECT '' AS five, f1 FROM FLOAT8_TBL ORDER BY 2;

SELECT '' AS four, f.f1 FROM FLOAT8_TBL f WHERE f.f1 <> '1004.3' ORDER BY 2;

SELECT '' AS one, f.f1 FROM FLOAT8_TBL f WHERE f.f1 = '1004.3' ORDER BY 2;

SELECT '' AS three, f.f1 FROM FLOAT8_TBL f WHERE '1004.3' > f.f1 ORDER BY 2;

SELECT '' AS three, f.f1 FROM FLOAT8_TBL f WHERE  f.f1 < '1004.3' ORDER BY 2;

SELECT '' AS four, f.f1 FROM FLOAT8_TBL f WHERE '1004.3' >= f.f1 ORDER BY 2;

SELECT '' AS four, f.f1 FROM FLOAT8_TBL f WHERE  f.f1 <= '1004.3' ORDER BY 2;

SELECT '' AS three, f.f1, f.f1 * '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0' ORDER BY 2;

SELECT '' AS three, f.f1, f.f1 + '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0' ORDER BY 2;

SELECT '' AS three, f.f1, f.f1 / '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0' ORDER BY 2;

SELECT '' AS three, f.f1, f.f1 - '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0' ORDER BY 2;

SELECT '' AS one, f.f1 ^ '2.0' AS square_f1
   FROM FLOAT8_TBL f where f.f1 = '1004.3';

-- absolute value
SELECT '' AS five, f.f1, @f.f1 AS abs_f1
   FROM FLOAT8_TBL f;

-- truncate
SELECT '' AS five, f.f1, trunc(f.f1) AS trunc_f1
   FROM FLOAT8_TBL f ORDER BY 2;

-- round
SELECT '' AS five, f.f1, round(f.f1) AS round_f1
   FROM FLOAT8_TBL f ORDER BY 2;

-- ceil / ceiling
select ceil(f1) as ceil_f1 from float8_tbl f ORDER BY 1;
select ceiling(f1) as ceiling_f1 from float8_tbl f ORDER BY 1;

-- floor
select floor(f1) as floor_f1 from float8_tbl f ORDER BY 1;

-- sign
select sign(f1) as sign_f1 from float8_tbl f ORDER BY 1;

-- square root
SELECT sqrt(float8 '64') AS eight;

SELECT |/ float8 '64' AS eight;

SELECT '' AS three, f.f1, |/f.f1 AS sqrt_f1
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0' ORDER BY 2;

-- power
SELECT power(float8 '144', float8 '0.5');

-- take exp of ln(f.f1)
SELECT '' AS three, f.f1, exp(ln(f.f1)) AS exp_ln_f1
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0' ORDER BY 2;

-- cube root
SELECT ||/ float8 '27' AS three;

SELECT '' AS five, f.f1, ||/f.f1 AS cbrt_f1 FROM FLOAT8_TBL f ORDER BY 2;


SELECT '' AS five, f1 FROM FLOAT8_TBL ORDER BY 2;

UPDATE FLOAT8_TBL
   SET f1 = FLOAT8_TBL.f1 * '-1'
   WHERE FLOAT8_TBL.f1 > '0.0';

SELECT '' AS bad, f.f1 * '1e200' from FLOAT8_TBL f;

SELECT '' AS bad, f.f1 ^ '1e200' from FLOAT8_TBL f;

SELECT 0 ^ 0 + 0 ^ 1 + 0 ^ 0.0 + 0 ^ 0.5;

SELECT '' AS bad, ln(f.f1) from FLOAT8_TBL f where f.f1 = '0.0' ;

SELECT '' AS bad, ln(f.f1) from FLOAT8_TBL f where f.f1 < '0.0' ;

SELECT '' AS bad, exp(f.f1) from FLOAT8_TBL f;

SELECT '' AS bad, f.f1 / '0.0' from FLOAT8_TBL f;

SELECT '' AS five, f1 FROM FLOAT8_TBL ORDER BY 2;

-- test for over- and underflow
INSERT INTO FLOAT8_TBL(f1) VALUES ('10e400');

INSERT INTO FLOAT8_TBL(f1) VALUES ('-10e400');

INSERT INTO FLOAT8_TBL(f1) VALUES ('1e309');

INSERT INTO FLOAT8_TBL(f1) VALUES ('10e-400');

INSERT INTO FLOAT8_TBL(f1) VALUES ('-10e-400');

INSERT INTO FLOAT8_TBL(f1) VALUES ('1e-324');

INSERT INTO FLOAT8_TBL(f1) VALUES ('1e308');

INSERT INTO FLOAT8_TBL(f1) VALUES ('1e-323');

INSERT INTO FLOAT8_TBL(f1) VALUES ('+INFINITY'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('+InFiNiTY'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('+Inf'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('-INFINITY'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('-InFiNiTY'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('-Inf'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('NaN'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('+naN'::float8);

INSERT INTO FLOAT8_TBL(f1) VALUES ('-naN'::float8);

-- test for over- and underflow with update statement
UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='1e-324'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='1e309'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='1e-400'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='1e400'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='0.0'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='+INFINITY'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='+InFiNiTY'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='+Inf'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='-INFINITY'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='-Inf'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='NaN'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='+naN'::float8;

UPDATE FLOAT8_TBL SET f1='0.0'::float8 WHERE f1='-naN'::float8;

-- test for over- and underflow with delete statement
DELETE FROM FLOAT8_TBL WHERE f1='1e-324'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='1e309'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='1e400'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='1e-400'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='0.0'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='+INFINITY'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='+InFiNiTY'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='+Inf'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='-INFINITY'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='-Inf'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='-naN'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='+naN'::float8;

DELETE FROM FLOAT8_TBL WHERE f1='NaN'::float8;

-- maintain external table consistency across platforms
-- delete all values and reinsert well-behaved ones

DELETE FROM FLOAT8_TBL;

INSERT INTO FLOAT8_TBL(f1) VALUES ('0.0');

INSERT INTO FLOAT8_TBL(f1) VALUES ('-34.84');

INSERT INTO FLOAT8_TBL(f1) VALUES ('-1004.30');

INSERT INTO FLOAT8_TBL(f1) VALUES ('-1.2345678901234e+200');

INSERT INTO FLOAT8_TBL(f1) VALUES ('-1.2345678901234e-200');

SELECT '' AS five, f1 FROM FLOAT8_TBL ORDER BY 2;

-- test if you can dump/restore subnormal (1e-323) values
-- using COPY

CREATE TABLE FLOATS(a float8);

INSERT INTO FLOATS select 1e-307::float8 / 10^i FROM generate_series(1,16) i;

SELECT * FROM FLOATS ORDER BY a;

SELECT float8in(float8out(a)) FROM FLOATS ORDER BY a;

COPY FLOATS TO '/tmp/floats';

TRUNCATE FLOATS;

COPY FLOATS FROM '/tmp/floats';

SELECT * FROM FLOATS ORDER BY a;

-- test edge-case coercions to integer
SELECT '32767.4'::float8::int2;
SELECT '32767.6'::float8::int2;
SELECT '-32768.4'::float8::int2;
SELECT '-32768.6'::float8::int2;
SELECT '2147483647.4'::float8::int4;
SELECT '2147483647.6'::float8::int4;
SELECT '-2147483648.4'::float8::int4;
SELECT '-2147483648.6'::float8::int4;
SELECT '9223372036854773760'::float8::int8;
SELECT '9223372036854775807'::float8::int8;
SELECT '-9223372036854775808.5'::float8::int8;
SELECT '-9223372036854780000'::float8::int8;