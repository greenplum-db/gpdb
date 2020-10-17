--
-- leastsquares.sql - test for ordinary least squares regression aggregates:
--   * regr_count
--   * regr_avgx
--   * regr_avgy
--   * regr_sxx
--   * regr_sxy
--   * regr_syy
--   * regr_intercept
--   * regr_slope
--   * regr_r2

CREATE TABLE weibull
(
	id INTEGER NOT NULL,
	x1 DOUBLE PRECISION,
	x2 DOUBLE PRECISION,
	y DOUBLE PRECISION
) DISTRIBUTED BY (id);

COPY weibull (id, x1, x2, y) FROM stdin;
1	41.9	29.1	251.3
2	43.4	29.3	251.3
3	43.9	29.5	248.3
4	44.5	29.7	267.5
5	47.3	29.9	273.0
6	47.5	30.3	276.5
7	47.9	30.5	270.3
8	50.2	30.7	274.9
9	52.8	30.8	285.0
10	53.2	30.9	290.0
11	56.7	31.5	297.0
12	57.0	31.7	302.5
13	63.5	31.9	304.5
14	65.3	32.0	309.3
15	71.1	32.1	321.7
16	77.0	32.5	330.7
17	77.8	32.9	349.0
\.

--
-- Testing of basic single linear regression code
--
-- these queries should produce the same result.
select 
    regr_count(y, x1)::real as count,
    regr_avgx(y, x1)::real as avgx,
    regr_avgy(y, x1)::real as avgy,
    regr_sxx(y, x1)::real as sxx,
    regr_sxy(y, x1)::real as sxy,
    regr_syy(y, x1)::real as syy,
    regr_intercept(y, x1)::real as intercept,
    regr_slope(y, x1)::real as slope,
    regr_r2(y, x1)::real as r2
from weibull;

select
    count(y)::real as count,
    avg(x1)::real as avgx,
    avg(y)::real as avgy,
    ((count(y) * sum(x1*x1) - sum(x1)^2)/count(y))::real as sxx,
    ((count(y) * sum(x1*y) - sum(x1)*sum(y))/count(y))::real as sxy,
    ((count(y) * sum(y*y) - sum(y)^2)/count(y))::real as sxy,
    ((sum(y)*sum(x1*x1) - sum(x1)*sum(x1*y))/(count(y) * sum(x1*x1) - sum(x1)^2))::real as intercept,
    ((count(y) * sum(x1*y) - sum(x1)*sum(y))/(count(y) * sum(x1*x1) - sum(x1)^2))::real as slope,
    ((count(y) * sum(x1*y) - sum(x1)*sum(y))^2/
        ((count(y) * sum(x1*x1) - sum(x1)^2) * (count(y) * sum(y*y) - sum(y)^2)))::real as r2
from weibull;

-- Single linear and multivariate should match for a single independent variable
select 
    array[regr_intercept(y, x1), regr_slope(y, x1)]::real[] as coef,
    regr_r2(y,x1)::real as r2
from weibull;

select 
    array[regr_intercept(y, x2), regr_slope(y, x2)]::real[] as coef,
    regr_r2(y,x2)::real as r2
from weibull;

DROP TABLE weibull;
