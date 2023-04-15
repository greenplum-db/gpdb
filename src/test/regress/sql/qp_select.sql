SET optimizer_trace_fallback TO ON;

-- test that interval constraints are derived correctly
-- start_ignore
DROP TABLE IF EXISTS qp_select;
CREATE TABLE qp_select(a int);
INSERT INTO qp_select VALUES (1), (2), (4), (8), (16), (32), (64), (128), (256);
-- end_ignore

-- basic
SELECT * FROM qp_select WHERE 1 + 15 >= a AND 1 - 15 <= a;
SELECT * FROM qp_select WHERE a + 15 >= a AND a - 15 <= a;
SELECT * FROM qp_select WHERE a + 15 <= a AND a - 15 >= a;
SELECT * FROM qp_select WHERE a + 0 <= a AND a - 0 >= a;

-- basic and arguments reversed
SELECT * FROM qp_select WHERE 1 - 15 <= a AND 1 + 15 >= a;
SELECT * FROM qp_select WHERE a - 15 <= a AND a + 15 >= a;
SELECT * FROM qp_select WHERE a - 15 >= a AND a + 15 <= a;
SELECT * FROM qp_select WHERE a - 0 >= a AND a + 0 <= a;

-- basic non-eq
SELECT * FROM qp_select WHERE 1 + 15 > a AND 1 - 15 < a;
SELECT * FROM qp_select WHERE a + 15 > a AND a - 15 < a;
SELECT * FROM qp_select WHERE a + 15 < a AND a - 15 > a;
SELECT * FROM qp_select WHERE a + 0 < a AND a - 0 > a;

-- basic + or
SELECT * FROM qp_select WHERE 1 + 15 >= a AND 1 - 15 <= a OR a > 5;
SELECT * FROM qp_select WHERE a + 15 >= a AND a - 15 <= a OR a > 5;
SELECT * FROM qp_select WHERE a + 15 <= a AND a - 15 >= a OR a > 5;
SELECT * FROM qp_select WHERE a + 0 < a AND a - 0 > a OR a > 5;

-- or + basic
SELECT * FROM qp_select WHERE a > 5 OR 1 + 15 >= a AND 1 - 15 <= a;
SELECT * FROM qp_select WHERE a > 5 OR a + 15 >= a AND a - 15 <= a;
SELECT * FROM qp_select WHERE a > 5 OR a + 15 <= a AND a - 15 >= a;
SELECT * FROM qp_select WHERE a > 5 OR a + 0 < a AND a - 0 > a;

--or
SELECT * FROM qp_select WHERE 1 + 15 >= a OR 1 - 15 <= a;
SELECT * FROM qp_select WHERE a + 15 >= a OR a - 15 <= a;
SELECT * FROM qp_select WHERE a + 15 <= a OR a - 15 >= a;
SELECT * FROM qp_select WHERE a + 0 <= a OR a - 0 >= a;

--or eq
SELECT * FROM qp_select WHERE 1 + 15 = a OR 1 - 15 = a;
SELECT * FROM qp_select WHERE a + 15 = a OR a - 15 = a;
SELECT * FROM qp_select WHERE a + 15 = a OR a - 15 = a;
SELECT * FROM qp_select WHERE a + 0 = a OR a - 0 = a;

-- basic commutative operator
SELECT * FROM qp_select WHERE 1 + 15 <= a AND 1 - 15 >= a;
SELECT * FROM qp_select WHERE a + 15 <= a AND a - 15 >= a;
SELECT * FROM qp_select WHERE a + 15 >= a AND a - 15 <= a;
SELECT * FROM qp_select WHERE a + 0 >= a AND a - 0 <= a;

-- basic swap position (left vs right) of compare arguments
SELECT * FROM qp_select WHERE a >= 1 + 15 AND a <= 1 - 15;
SELECT * FROM qp_select WHERE a >= a + 15 AND a <= a - 15;
SELECT * FROM qp_select WHERE a <= a + 15 AND a >= a - 15;
SELECT * FROM qp_select WHERE a <= a + 0 AND a >= a - 0;

-- <> operator
SELECT * FROM qp_select WHERE 1 + 15 <> a AND 1 - 15 <> a;
SELECT * FROM qp_select WHERE a + 15 <> a AND a - 15 <> a;
SELECT * FROM qp_select WHERE a + 15 <> a AND a - 15 <> a;
SELECT * FROM qp_select WHERE a + 0 <> a AND a - 0 <> a;

RESET optimizer_trace_fallback;
