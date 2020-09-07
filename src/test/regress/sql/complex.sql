--
-- complex
--
-- start_ignore
DROP TABLE complex_ttbl;   
DROP SEQUENCE  complex_seq;
-- end_ignore
CREATE SEQUENCE complex_seq CACHE 1;
CREATE TABLE complex_ttbl (id INTEGER NOT NULL DEFAULT 1, orderid INTEGER NOT NULL DEFAULT NEXTVAL('complex_seq'), c COMPLEX) DISTRIBUTED BY (id); 

-- regular input
INSERT INTO complex_ttbl(c) VALUES (' 5');
INSERT INTO complex_ttbl(c) VALUES ('3 ');
INSERT INTO complex_ttbl(c) VALUES (' 6 ');
INSERT INTO complex_ttbl(c) VALUES (' -6 ');
INSERT INTO complex_ttbl(c) VALUES (' 5i');
INSERT INTO complex_ttbl(c) VALUES ('3i ');
INSERT INTO complex_ttbl(c) VALUES (' 6i ');
INSERT INTO complex_ttbl(c) VALUES (' -6i ');
INSERT INTO complex_ttbl(c) VALUES (' 5 + 3i ');
INSERT INTO complex_ttbl(c) VALUES (' 5 + -3i ');
INSERT INTO complex_ttbl(c) VALUES (' 6 - -7i ');
INSERT INTO complex_ttbl(c) VALUES (' -6 - -7i ');
INSERT INTO complex_ttbl(c) VALUES (' 1.2345678901234e+200  - -1.2345678901234e+200i ');
INSERT INTO complex_ttbl(c) VALUES (' -0 + -0i ');
INSERT INTO complex_ttbl(c) VALUES (' -0 - -0i ');
INSERT INTO complex_ttbl(c) VALUES (' -0 - 0i ');
INSERT INTO complex_ttbl(c) VALUES ('9+10i ');
INSERT INTO complex_ttbl(c) VALUES (' 9+10i');
INSERT INTO complex_ttbl(c) VALUES ('9-10i');

-- special input
INSERT INTO COMPLEX_ttbl(c) VALUES ('infinity + infinityi');
INSERT INTO COMPLEX_ttbl(c) VALUES ('infinity - infinityi');
INSERT INTO COMPLEX_ttbl(c) VALUES ('-InFiNity - -infinityi');
INSERT INTO COMPLEX_ttbl(c) VALUES ('-infinity + -InFinItyi');

INSERT INTO COMPLEX_ttbl(c) VALUES ('nan + nani');
INSERT INTO COMPLEX_ttbl(c) VALUES ('nan - nani');
INSERT INTO COMPLEX_ttbl(c) VALUES ('-nan - -NAni');
INSERT INTO COMPLEX_ttbl(c) VALUES ('-nAn + -naNi');

SELECT id, c FROM complex_ttbl ORDER BY orderid;
-- test for underflow and overflow
SELECT COMPLEX '10e400';
SELECT COMPLEX '10e400i';
SELECT COMPLEX '-10e400';
SELECT COMPLEX '-10e400i';
SELECT COMPLEX '10e-400';
SELECT COMPLEX '10e-400i';
SELECT COMPLEX '-10e-400';
SELECT COMPLEX '-10e-400i';

-- bad input  
INSERT INTO complex_ttbl(c) VALUES ('');
INSERT INTO complex_ttbl(c) VALUES ('    ');
INSERT INTO complex_ttbl(c) VALUES ('xyi');
INSERT INTO complex_ttbl(c) VALUES ('i'); 
INSERT INTO complex_ttbl(c) VALUES ('5.0.0 + 1i'); 
INSERT INTO complex_ttbl(c) VALUES ('5.0 + 1 i');  
INSERT INTO complex_ttbl(c) VALUES ('5.0 + i1');    

-- re
SELECT re(COMPLEX(5, 3)) AS five;
SELECT re(COMPLEX(-5, 3)) AS minus_five;
SELECT re(COMPLEX(5.1, 3)) AS five_point_one;
SELECT re(COMPLEX(-5.1, 3))  AS minus_five_point_one;
SELECT re(COMPLEX('infinity', 3)) AS inf;
SELECT re(COMPLEX('-infinity', 3)) AS minus_inf;
SELECT re(COMPLEX('nan', 3)) AS nan;
SELECT re(COMPLEX('-nan', 3)) AS nan;
SELECT re(COMPLEX('nan', 3)) AS nan;
SELECT re(COMPLEX('-nan', 3)) AS nan;

-- im
SELECT im(COMPLEX(5, 3)) AS three;
SELECT im(COMPLEX(5, -3)) AS minus_three;
SELECT im(COMPLEX(5, 3.1)) AS three_point_1;
SELECT im(COMPLEX(5, -3.1)) AS minus_three_point_1;
SELECT im(COMPLEX(5, 'infinity')) AS inf;
SELECT im(COMPLEX(5, '-infinity')) AS minus_inf;
SELECT im(COMPLEX(5, 'nan')) AS nan;
SELECT im(COMPLEX(5, '-nan')) AS nan;
SELECT im(COMPLEX(5, 'nan')) AS nan;
SELECT im(COMPLEX(5, '-nan')) AS nan;

-- abs
SELECT abs(COMPLEX(4, 3)) AS five;
SELECT abs(COMPLEX(4, -3)) AS five;
SELECT abs(COMPLEX(-4, 3)) AS five;
SELECT abs(COMPLEX(-4, -3)) AS five;

SELECT abs(COMPLEX('infinity', 3)) AS inf;
SELECT abs(COMPLEX('-infinity', 3)) AS inf;
SELECT abs(COMPLEX(5, 'infinity')) AS inf;
SELECT abs(COMPLEX(5, '-infinity')) AS inf;
SELECT abs(COMPLEX('infinity', 'infinity')) AS inf;

SELECT abs(COMPLEX('nan', 3)) AS nan;
SELECT abs(COMPLEX('-nan', 3))  AS nan;
SELECT abs(COMPLEX(5, 'nan'))  AS nan;
SELECT abs(COMPLEX(5, '-nan'))  AS nan;
SELECT abs(COMPLEX('nan', 'nan'))  AS nan;

SELECT abs(COMPLEX('infinity', 'nan')) AS inf;
SELECT abs(COMPLEX('nan', 'infinity')) AS inf;

-- radians
SELECT abs(radians(COMPLEX(1, sqrt(3))) - pi()/3) < 1e-6 AS tr;
SELECT abs(radians(COMPLEX(1, -sqrt(3))) + pi()/3) < 1e-6 AS tr;
SELECT abs(radians(COMPLEX(-1, sqrt(3))) - pi()*2/3) < 1e-6 AS tr;
SELECT abs(radians(COMPLEX(-1, -sqrt(3))) + pi()*2/3) < 1e-6 AS tr;

SELECT radians(COMPLEX('infinity', 3)) AS zero;
SELECT abs(radians(COMPLEX('-infinity', 3)) - pi()) < 1e-6 AS tr;
SELECT abs(radians(COMPLEX(5, 'infinity')) - pi()/2) < 1e-6 AS tr;
SELECT abs(radians(COMPLEX(5, '-infinity')) + pi()/2) < 1e-6 AS tr;
SELECT abs(radians(COMPLEX('infinity', 'infinity')) - pi()*45/180) < 1e-6 AS tr;

SELECT radians(COMPLEX('nan', 3)) AS nan;
SELECT radians(COMPLEX('-nan', 3)) AS nan;
SELECT radians(COMPLEX(5, 'nan')) AS nan;
SELECT radians(COMPLEX(5, '-nan')) AS nan;
SELECT radians(COMPLEX('nan', 'nan')) AS nan; 
SELECT radians(COMPLEX('infinity', 'nan')) AS nan;
SELECT radians(COMPLEX('nan', 'infinity')) AS nan;

-- conj
SELECT conj(COMPLEX(5, 3)) = (COMPLEX(5,-3)) AS tr;
SELECT conj(COMPLEX(5, -3)) = (COMPLEX(5,3)) AS tr;
SELECT conj(COMPLEX(5, 3.1)) = (COMPLEX(5,-3.1)) AS tr;
SELECT conj(COMPLEX(5, -3.1)) = (COMPLEX(5,3.1)) AS tr;
SELECT conj(COMPLEX(5, 'infinity')) = (COMPLEX(5,'-infinity')) AS tr;
SELECT conj(COMPLEX(5, '-infinity')) = (COMPLEX(5,'infinity')) AS tr;
SELECT conj(COMPLEX(5, 'nan')) = (COMPLEX(5,'nan')) AS tr;
SELECT conj(COMPLEX(5, '-nan')) = (COMPLEX(5,'nan')) AS tr;
SELECT conj(COMPLEX(5, 'nan')) = (COMPLEX(5,'nan')) AS tr;
SELECT conj(COMPLEX(5, '-nan')) = (COMPLEX(5,'nan')) AS tr;

-- hashcomplex
SELECT hashcomplex(COMPLEX(0,0)) = hashcomplex(COMPLEX(0,-0)) AS tr;
SELECT hashcomplex(COMPLEX(0,0)) = hashcomplex(COMPLEX(-0,0)) AS tr;
SELECT hashcomplex(COMPLEX(0,0)) = hashcomplex(COMPLEX(-0,-0)) AS tr;

-- not equal
SELECT NOT c != c AS tr FROM complex_ttbl;

-- unary plus
SELECT +(COMPLEX(5, 3)) = (COMPLEX(5, 3)) AS tr;
SELECT +(COMPLEX(-5, 3)) = (COMPLEX(-5, 3)) AS tr;
SELECT +(COMPLEX(5.1, 3)) = (COMPLEX(5.1, 3)) AS tr;
SELECT +(COMPLEX(-5.1, 3)) = (COMPLEX(-5.1, 3)) AS tr;
SELECT +(COMPLEX('infinity', 3)) = (COMPLEX('infinity', 3)) AS tr;
SELECT +(COMPLEX('-infinity', 3)) = (COMPLEX('-infinity', 3)) AS tr;
SELECT +(COMPLEX('nan', 3)) = (COMPLEX('nan', 3)) AS tr;
SELECT +(COMPLEX('-nan', 3)) = (COMPLEX('-nan', 3)) AS tr;
SELECT +(COMPLEX('nan', 3)) = (COMPLEX('-nan', 3)) AS tr;
SELECT +(COMPLEX('-nan', 3)) = (COMPLEX('nan', 3)) AS tr;

SELECT +(COMPLEX(5, 3)) = (COMPLEX(5,3)) AS tr;
SELECT +(COMPLEX(5, -3)) = (COMPLEX(5,-3)) AS tr;
SELECT +(COMPLEX(5, 3.1)) = (COMPLEX(5,3.1)) AS tr;
SELECT +(COMPLEX(5, -3.1)) = (COMPLEX(5,-3.1)) AS tr;
SELECT +(COMPLEX(5, 'infinity')) = (COMPLEX(5,'infinity')) AS tr;
SELECT +(COMPLEX(5, '-infinity')) = (COMPLEX(5,'-infinity')) AS tr;
SELECT +(COMPLEX(5, 'nan')) = (COMPLEX(5,'nan')) AS tr;
SELECT +(COMPLEX(5, '-nan')) = (COMPLEX(5,'-nan')) AS tr;
SELECT +(COMPLEX(5, 'nan')) = (COMPLEX(5,'-nan')) AS tr;
SELECT +(COMPLEX(5, '-nan')) = (COMPLEX(5,'nan')) AS tr;


-- unary minus
SELECT -(COMPLEX(5, 3)) = (COMPLEX(-5, -3)) AS tr;
SELECT -(COMPLEX(-5, 3)) = (COMPLEX(5, -3)) AS tr;
SELECT -(COMPLEX(5.1, 3)) = (COMPLEX(-5.1, -3)) AS tr;
SELECT -(COMPLEX(-5.1, 3)) = (COMPLEX(5.1, -3)) AS tr;
SELECT -(COMPLEX('infinity', 3)) = (COMPLEX('-infinity', -3)) AS tr;
SELECT -(COMPLEX('-infinity', 3)) = (COMPLEX('infinity', -3)) AS tr;
SELECT -(COMPLEX('nan', 3)) = (COMPLEX('nan', -3)) AS tr;
SELECT -(COMPLEX('-nan', 3)) = (COMPLEX('nan', -3)) AS tr;
SELECT -(COMPLEX('nan', 3)) = (COMPLEX('nan', -3)) AS tr;
SELECT -(COMPLEX('-nan', 3)) = (COMPLEX('nan', -3)) AS tr;
SELECT -(COMPLEX(5, 'infinity')) = (COMPLEX(-5,'-infinity')) AS tr;
SELECT -(COMPLEX(5, '-infinity')) = (COMPLEX(-5,'infinity')) AS tr;
SELECT -(COMPLEX(5, 'nan')) = (COMPLEX(-5,'nan')) AS tr;
SELECT -(COMPLEX(5, '-nan')) = (COMPLEX(-5,'nan')) AS tr;
SELECT -(COMPLEX(5, 'nan')) = (COMPLEX(-5,'nan')) AS tr;
SELECT -(COMPLEX(5, '-nan')) = (COMPLEX(-5,'nan')) AS tr;

SELECT c + -c = COMPLEX(0,0) 
	FROM complex_ttbl 
	WHERE re(c) <> FLOAT8 'nan' AND im(c) <> FLOAT8 'nan' AND abs(re(c)) <> FLOAT8 'infinity' AND abs(im(c)) <> FLOAT8 'infinity';

-- plus
SELECT COMPLEX(3, 5) + COMPLEX(6, 7) = COMPLEX(9,12) AS tr;
SELECT COMPLEX(3, 'infinity') + COMPLEX(6, 7) = COMPLEX(9, 'infinity') AS tr;
SELECT COMPLEX('infinity', 5) + COMPLEX(6, 7) = COMPLEX('infinity', 12) AS tr;
SELECT COMPLEX(3, '-infinity') + COMPLEX(6, 7) = COMPLEX(9, '-infinity') AS tr;
SELECT COMPLEX('-infinity', 5) + COMPLEX(6, 7) = COMPLEX('-infinity', 12) AS tr;

SELECT COMPLEX(3, 'nan') + COMPLEX(6, 7) = COMPLEX(9, 'nan') AS tr;
SELECT COMPLEX('nan', 5) + COMPLEX(6, 7) = COMPLEX('nan', 12) AS tr;
SELECT COMPLEX(3, '-nan') + COMPLEX(6, 7) = COMPLEX(9, 'nan') AS tr;
SELECT COMPLEX('-nan', 5) + COMPLEX(6, 7) = COMPLEX('nan', 12) AS tr;

SELECT COMPLEX(3, 'nan') + COMPLEX(6, 'infinity') = COMPLEX(9, 'nan') AS tr;
SELECT COMPLEX('nan', 5) + COMPLEX('infinity', 7) = COMPLEX('nan', 12) AS tr;

-- minus
SELECT COMPLEX(3, 5) - COMPLEX(6, 7) = COMPLEX(-3, -2) AS tr;
SELECT COMPLEX(3, 'infinity') - COMPLEX(6, 7) = COMPLEX(-3, 'infinity') AS tr;
SELECT COMPLEX('infinity', 5) - COMPLEX(6, 7) = COMPLEX('infinity', -2) AS tr;
SELECT COMPLEX(3, '-infinity') - COMPLEX(6, 7) = COMPLEX(-3, '-infinity') AS tr;
SELECT COMPLEX('-infinity', 5) - COMPLEX(6, 7) = COMPLEX('-infinity', -2) AS tr;

SELECT COMPLEX(3, 'nan') - COMPLEX(6, 7) = COMPLEX(-3, 'nan') AS tr;
SELECT COMPLEX('nan', 5) - COMPLEX(6, 7) = COMPLEX('nan', -2) AS tr;
SELECT COMPLEX(3, '-nan') - COMPLEX(6, 7) = COMPLEX(-3, 'nan') AS tr;
SELECT COMPLEX('-nan', 5) - COMPLEX(6, 7) = COMPLEX('nan', -2) AS tr;

SELECT COMPLEX(3, 'nan') - COMPLEX(6, 'infinity') = COMPLEX(-3, 'nan') AS tr;
SELECT COMPLEX('nan', 5) - COMPLEX('infinity', 7) = COMPLEX('nan', -2) AS tr;

-- multiply
SELECT COMPLEX(3, 5) * COMPLEX(6, 7) = COMPLEX(-17, 51) AS tr;
SELECT COMPLEX(3, 'infinity') * COMPLEX(6, 7) = COMPLEX('-infinity', 'infinity') AS tr;
SELECT COMPLEX('infinity', 5) * COMPLEX(6, 7) = COMPLEX('infinity', 'infinity') AS tr;
SELECT COMPLEX(3, '-infinity') * COMPLEX(6, 7) = COMPLEX('infinity', '-infinity') AS tr;
SELECT COMPLEX('-infinity', 5) * COMPLEX(6, 7) = COMPLEX('-infinity', '-infinity') AS tr;

SELECT COMPLEX(3, 'nan') * COMPLEX(6, 7) = COMPLEX('nan', 'nan') AS tr;
SELECT COMPLEX('nan', 5) * COMPLEX(6, 7) = COMPLEX('nan', 'nan') AS tr;
SELECT COMPLEX(3, '-nan') * COMPLEX(6, 7) = COMPLEX('nan', 'nan') AS tr;
SELECT COMPLEX('-nan', 5) * COMPLEX(6, 7) = COMPLEX('nan', 'nan') AS tr;

SELECT COMPLEX(3, 'nan') * COMPLEX(6, 'infinity') = COMPLEX('nan', 'nan') AS tr;
SELECT COMPLEX('nan', 5) * COMPLEX('infinity', 7) = COMPLEX('nan', 'nan') AS tr;

-- divide
CREATE OR REPLACE FUNCTION complex_dp_eq(a COMPLEX, b COMPLEX, diff FLOAT8) RETURNS BOOLEAN AS $$
BEGIN
	RETURN (abs(re(a) - re(b)) < diff) AND (abs(im(a) - im(b)) < diff);
END;
$$ LANGUAGE PLPGSQL IMMUTABLE STRICT;

SELECT COMPLEX(2,2)/COMPLEX(1,1) = COMPLEX(2,0) AS tr;

SELECT COMPLEX(3, 'infinity') / COMPLEX(6, 7) = COMPLEX('infinity', 'infinity') AS tr;

SELECT COMPLEX(3, 'nan') / COMPLEX(6, 7) = COMPLEX('nan', 'nan') AS tr;

-- for division the denominator is special
SELECT COMPLEX(3, 5) / COMPLEX('infinity', 7) = COMPLEX('NaN','NaN') AS tr;

-- divived by 0, python not supported divided by zero, and different version of glibc have different behavior
SELECT COMPLEX(5,3)/COMPLEX(0,0) = COMPLEX('nan', 'nan') AS tr;

SELECT COMPLEX('infinity',3)/COMPLEX(0,0) = COMPLEX('nan', 'nan') AS tr;

SELECT COMPLEX(5,'nan')/COMPLEX(0,0) = COMPLEX('infinity', 'nan') AS tr;

-- @
SELECT @(COMPLEX(5,3)) = abs(COMPLEX(5,3)) AS tr;

-- pow and ^
SELECT complex_dp_eq(COMPLEX(1,sqrt(3))^3 , COMPLEX(-1*2^3, 0), 1e-6) AS tr;

SELECT complex_dp_eq(COMPLEX(0.5, 0.5*sqrt(3))^0.5, COMPLEX(0.5*sqrt(3), 0.5), 1e-6) AS tr;

SELECT COMPLEX(5,3)^0 = COMPLEX(1,0) AS tr;

SELECT complex_dp_eq(COMPLEX(5,3)^(-0.5) , COMPLEX(1,0)/(COMPLEX(5,3)^0.5), 1e-6) AS tr;

SELECT complex_dp_eq(COMPLEX(5,3)^(-3) , COMPLEX(1,0)/COMPLEX(5,3)^3, 1e-6) AS tr;

SELECT complex_dp_eq(COMPLEX(5,3)^COMPLEX(3,5), COMPLEX(-7.04464, -11.27606), 1e-5) AS tr;

SELECT complex_dp_eq(5^COMPLEX(3,5), COMPLEX(-24.00101, 122.67416), 1e-5) AS tr;

SELECT COMPLEX(5,3)^COMPLEX(3,5) = power(COMPLEX(5,3), COMPLEX(3,5)) AS tr;

SELECT (-1)^0.5 AS wrong;

SELECT power(-1, 0.5) AS wrong;

SELECT complex_dp_eq(power(-1::COMPLEX, 0.5), COMPLEX(0, -1), 1e-6) AS tr;
SELECT complex_dp_eq(power(COMPLEX(-1, 0), 0.5), COMPLEX(0,1), 1e-6) AS tr;
SELECT complex_dp_eq(power('-1 + -0i'::COMPLEX, 0.5), COMPLEX(0, -1), 1e-6) AS tr;
SELECT complex_dp_eq(power('-1 - 0i'::COMPLEX, 0.5), COMPLEX(0, -1), 1e-6) AS tr;
SELECT complex_dp_eq(power('-1 + 0i'::COMPLEX, 0.5), COMPLEX(0,1), 1e-6) AS tr;

-- sqrt
SELECT sqrt(COMPLEX(5,3)) = power(COMPLEX(5,3), 0.5) AS tr;

-- cbrt
SELECT cbrt(COMPLEX(5,3)) = power(COMPLEX(5,3), (1.0/3)) AS tr;

-- degrees
SELECT degrees(COMPLEX(5,3)) = degrees(radians(COMPLEX(5,3))) AS tr;

-- exp
SELECT complex_dp_eq(exp(COMPLEX(5,3)), COMPLEX(-146.927913908319 , 20.944066208746), 1e-6) AS tr;

-- ln
SELECT complex_dp_eq(ln(COMPLEX(5,3)), COMPLEX(1.76318026230808 , 0.540419500270584), 1e-6) AS tr;

SELECT complex_dp_eq(exp(ln(COMPLEX(5,3))), COMPLEX(5,3), 1e-6) AS tr;

SELECT ln(0::COMPLEX) = COMPLEX('-infinity', 0) AS tr;

-- log10
SELECT complex_dp_eq(log(COMPLEX(5,3)) , ln(COMPLEX(5,3))/ln(10), 1e-6) AS tr;

-- log
SELECT complex_dp_eq(log(COMPLEX(3,5),COMPLEX(5,3)) , ln(COMPLEX(5,3))/ln(COMPLEX(3,5)), 1e-6) AS tr;

-- acos
SELECT complex_dp_eq(acos(COMPLEX(5,3)), COMPLEX(0, -1)*ln(COMPLEX(5,3) + COMPLEX(0,1)*sqrt(1 - COMPLEX(5,3)^2)), 1e-6) AS tr;

-- asin
SELECT complex_dp_eq(asin(COMPLEX(5,3)), COMPLEX(0, -1)*ln(COMPLEX(5,3)*COMPLEX(0,1) + sqrt(1 - COMPLEX(5,3)^2)), 1e-6) AS tr;

-- atan
SELECT complex_dp_eq(atan(COMPLEX(5,3)), 0.5*COMPLEX(0,1)*(ln(1 - COMPLEX(5,3)*COMPLEX(0,1)) - ln(1 + COMPLEX(5,3)*COMPLEX(0,1))), 1e-6) AS tr;

-- cos
SELECT complex_dp_eq(cos(COMPLEX(5,3)), COMPLEX( 2.85581500422739 , 9.60638344843258), 1e-6) AS tr;

-- sin
SELECT complex_dp_eq(sin(COMPLEX(5,3)), COMPLEX(-9.65412547685484 , 2.84169229560635), 1e-6) AS tr;

-- cot
SELECT complex_dp_eq(cot(COMPLEX(5,3)), cos(COMPLEX(5,3))/sin(COMPLEX(5,3)), 1e-6) AS tr;

-- tan
SELECT complex_dp_eq(tan(COMPLEX(5,3)), sin(COMPLEX(5,3))/cos(COMPLEX(5,3)), 1e-6) AS tr;

-- type cast
SELECT COMPLEX '5+3i' ^ 4::int2 = power(COMPLEX(5,3),4::COMPLEX) AS tr;
SELECT COMPLEX '5+3i' ^ 4::INT4 = power(COMPLEX(5,3),4::COMPLEX) AS tr;
SELECT COMPLEX '5+3i' ^ 4::INT8 = power(COMPLEX(5,3),4::COMPLEX) AS tr;
SELECT COMPLEX '5+3i' ^ 4 		= power(COMPLEX(5,3),4::COMPLEX) AS tr;

SELECT COMPLEX '5+3i' ^ 4.5::FLOAT4 = power(COMPLEX(5,3),COMPLEX(4.5,0)) AS tr;
SELECT COMPLEX '5+3i' ^ 4.5::FLOAT8 = power(COMPLEX(5,3),COMPLEX(4.5,0)) AS tr;

SELECT COMPLEX(5,4)::POINT = POINT(5,4) AS tr;

SELECT POINT(5,4)::COMPLEX = COMPLEX(5,4) AS tr;

-- clear up
DROP TABLE complex_ttbl;
DROP SEQUENCE complex_seq;
DROP FUNCTION complex_dp_eq(a COMPLEX, b COMPLEX, diff FLOAT8);

-- check hashable
CREATE SEQUENCE complex_seq CACHE 1;
CREATE TABLE complex_ttbl (id INT4 DEFAULT NEXTVAL('complex_seq'), c COMPLEX) DISTRIBUTED BY (c);

INSERT INTO complex_ttbl(c) VALUES (COMPLEX(NEXTVAL('complex_seq'), NEXTVAL('complex_seq')));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(NEXTVAL('complex_seq'), NEXTVAL('complex_seq')));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(NEXTVAL('complex_seq'), NEXTVAL('complex_seq')));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(NEXTVAL('complex_seq'), NEXTVAL('complex_seq')));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(NEXTVAL('complex_seq'), NEXTVAL('complex_seq')));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(NEXTVAL('complex_seq'), NEXTVAL('complex_seq')));

INSERT INTO complex_ttbl(c) VALUES (COMPLEX(0, 0));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(0, -0));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(-0, 0));
INSERT INTO complex_ttbl(c) VALUES (COMPLEX(-0, -0));

SELECT * FROM complex_ttbl ORDER BY id;

SELECT COUNT(c) = 4 AS tr, COUNT(DISTINCT gp_segment_id) = 1 AS tr 
	FROM complex_ttbl 
	WHERE re(c) = 0 AND im(c) = 0;

-- Use a complex in GROUP BY, to test the hash function used in hash agg.
set enable_groupagg = off;
SELECT c, count(*) FROM complex_ttbl GROUP BY c;
reset enable_groupagg;

DROP TABLE complex_ttbl;
DROP SEQUENCE complex_seq;

-- dot product
SELECT dotproduct(ARRAY[COMPLEX(1,3),COMPLEX(5,7)], ARRAY[COMPLEX(2,4),COMPLEX(6,8)]) = COMPLEX(1,3)*COMPLEX(2,4) + COMPLEX(5,7)*COMPLEX(6,8) AS tr;

SELECT dotproduct(ARRAY[COMPLEX(1,3),COMPLEX(5,7)], ARRAY[COMPLEX(2,4),NULL]) IS NULL AS tr;

SELECT dotproduct(ARRAY[COMPLEX(1,3),COMPLEX(5,7)], ARRAY[COMPLEX(2,4),COMPLEX(6,8), COMPLEX(10,12)]);

SELECT dotproduct(ARRAY[COMPLEX(1,3),COMPLEX(5,7)], ARRAY[ARRAY[COMPLEX(2,4)],ARRAY[COMPLEX(6,8)]]);


-- GIN index on complex array
CREATE TABLE complex_array_tab (complex_arr complex[]);
CREATE INDEX ON complex_array_tab USING gin (complex_arr);

INSERT INTO complex_array_tab VALUES (ARRAY[COMPLEX(1,3), COMPLEX(5,7)]);
INSERT INTO complex_array_tab VALUES (ARRAY[COMPLEX(2,4), COMPLEX(6,8)]);

SELECT * FROM complex_array_tab WHERE complex_arr @> ARRAY[COMPLEX(2,4)];
