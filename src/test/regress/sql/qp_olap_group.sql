-- ###### Queries involving MIN() function ###### --

SELECT DISTINCT sale.dt,sale.qty,GROUPING(sale.dt,sale.dt),GROUP_ID(), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn-sale.vn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY CUBE((sale.pn,sale.pn),(sale.pn),(sale.vn),(sale.cn,sale.dt,sale.cn),(sale.cn,sale.pn)),ROLLUP((sale.pn),(sale.qty,sale.dt),(sale.pn,sale.cn),(sale.vn,sale.dt),(sale.vn,sale.pn)),(sale.qty),(sale.prc),(sale.dt);

-- ###### Queries involving STDDEV() function ###### --

SELECT DISTINCT sale.vn, TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.cn*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_SAMP(floor(sale.qty+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(floor(sale.cn)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY GROUPING SETS(CUBE((sale.pn,sale.vn,sale.pn),(sale.cn,sale.pn,sale.vn))) HAVING COALESCE(AVG(sale.vn),0) <= 50.5023418504766 AND NOT COALESCE(COUNT(sale.vn),0) < 36.1996525709475;

-- ###### Queries involving STDDEV_POP() function ###### --

SELECT DISTINCT sale.vn,sale.vn,GROUPING(sale.vn),GROUP_ID(), TO_CHAR(COALESCE(STDDEV_POP(floor(sale.cn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.pn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.qty+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.cn-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.pn/sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY CUBE((sale.cn,sale.prc),(sale.vn),(sale.prc),(sale.cn)),CUBE((sale.cn,sale.cn,sale.qty),(sale.pn),(sale.pn,sale.dt,sale.cn)),ROLLUP((sale.pn),(sale.qty,sale.pn));

-- ###### Queries involving STDDEV_SAMP() function ###### --

SELECT DISTINCT sale.vn,sale.dt,sale.prc, TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.pn+sale.vn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY GROUPING SETS(CUBE((sale.pn),(sale.dt,sale.qty),(sale.vn,sale.vn,sale.cn),(sale.cn),(sale.pn,sale.vn))),ROLLUP((sale.cn,sale.dt,sale.prc),(sale.pn,sale.pn,sale.cn),(sale.dt,sale.vn,sale.pn),(sale.dt,sale.cn),(sale.dt,sale.prc,sale.pn),(sale.vn,sale.qty)),CUBE((sale.vn,sale.cn),(sale.qty,sale.prc),(sale.qty),(sale.cn,sale.vn),(sale.vn,sale.cn),(sale.qty,sale.cn));

-- ###### Queries involving SUM() function ###### --

SELECT DISTINCT sale.vn,sale.dt,sale.pn,sale.vn,sale.pn,GROUPING(sale.dt),GROUP_ID(), TO_CHAR(COALESCE(SUM(floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_SAMP(floor(sale.cn)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY CUBE((sale.qty),(sale.qty,sale.qty),(sale.vn),(sale.vn),(sale.cn),(sale.dt)),(sale.qty,sale.dt,sale.vn),(sale.pn),(sale.vn),() HAVING GROUP_ID() = 8;

-- ###### Queries involving VAR_POP() function ###### --

SELECT DISTINCT sale.dt,sale.pn,sale.vn,sale.cn,sale.pn,GROUPING(sale.pn,sale.pn,sale.vn,sale.cn), TO_CHAR(COALESCE(VAR_POP(floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.cn+sale.cn)),0),'99999999.9999999') 
FROM sale
GROUP BY GROUPING SETS((sale.vn)),sale.dt,sale.pn,sale.cn;

-- ###### Queries involving VAR_SAMP() function ###### --

SELECT DISTINCT sale.pn,sale.vn,sale.cn,sale.cn,GROUPING(sale.vn,sale.vn,sale.cn,sale.cn), TO_CHAR(COALESCE(VAR_SAMP(floor(sale.pn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.qty-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn/sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(floor(sale.qty-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_SAMP(floor(sale.qty+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn*sale.qty)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY CUBE((sale.prc,sale.cn,sale.dt),(sale.pn,sale.vn),(sale.cn),(sale.qty,sale.qty),(sale.vn,sale.pn));

-- ###### Queries involving VARIANCE() function ###### --

SELECT DISTINCT sale.cn,sale.qty,sale.vn,sale.qty,GROUPING(sale.cn,sale.qty),GROUP_ID(), TO_CHAR(COALESCE(VARIANCE(floor(sale.cn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(floor(sale.vn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY ROLLUP((sale.cn,sale.pn,sale.cn),(sale.cn,sale.qty),(sale.pn),(sale.prc,sale.qty),(sale.dt,sale.cn),(sale.pn)),CUBE((sale.vn),(sale.cn,sale.dt,sale.pn),(sale.pn),(sale.qty,sale.prc)),(sale.qty),(sale.qty),(sale.cn,sale.pn);

-- ###### Queries involving CORR() function ###### --

SELECT DISTINCT sale.pn,sale.cn,GROUPING(sale.cn),GROUP_ID(), TO_CHAR(COALESCE(CORR(floor(sale.pn-sale.cn),floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.cn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.prc)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((sale.vn),(sale.vn),(sale.qty)),sale.pn,sale.cn;

-- ###### Queries involving COVAR_POP() function ###### --
SELECT DISTINCT sale.qty,sale.pn,GROUPING(sale.pn,sale.pn), TO_CHAR(COALESCE(COVAR_POP(floor(sale.prc-sale.prc),floor(sale.vn-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.qty+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY (),sale.qty,sale.pn;

-- ###### Queries involving COVAR_SAMP() function ###### --

SELECT DISTINCT sale.qty,sale.qty,sale.cn,sale.cn,GROUP_ID(), TO_CHAR(COALESCE(COVAR_SAMP(floor(sale.pn),floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.pn*sale.qty)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY ROLLUP((sale.vn),(sale.vn,sale.cn)),sale.qty;

-- ###### Queries involving REGR_AVGX() function ###### --

SELECT DISTINCT sale.pn,sale.qty,GROUPING(sale.qty,sale.qty), TO_CHAR(COALESCE(REGR_AVGX(floor(sale.cn*sale.vn),floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.qty-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.prc/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.pn)),0),'99999999.9999999') 
FROM sale,customer
WHERE sale.cn=customer.cn
GROUP BY GROUPING SETS((),CUBE((sale.cn,sale.cn),(sale.pn),(sale.cn,sale.cn))),sale.qty;

-- ###### Queries involving REGR_AVGY() function ###### --

SELECT DISTINCT sale.qty,sale.vn,sale.cn,GROUPING(sale.cn,sale.vn,sale.vn), TO_CHAR(COALESCE(REGR_AVGY(floor(sale.pn*sale.cn),floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.cn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.qty+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.vn)),0),'99999999.9999999') 
FROM sale,customer
WHERE sale.cn=customer.cn
GROUP BY GROUPING SETS(()),sale.qty,sale.vn,sale.cn;

-- ###### Queries involving REGR_COUNT() function ###### --

SELECT DISTINCT sale.prc,sale.vn,sale.prc,GROUPING(sale.prc), TO_CHAR(COALESCE(REGR_COUNT(floor(sale.qty),floor(sale.prc+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.vn/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.cn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY ROLLUP((sale.qty,sale.dt,sale.qty)),sale.prc,sale.vn;

-- ###### Queries involving REGR_INTERCEPT() function ###### --

SELECT DISTINCT sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(sale.pn),floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_SAMP(floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(floor(sale.cn/sale.cn)),0),'99999999.9999999') 
FROM sale
GROUP BY (),(sale.qty,sale.dt,sale.cn),(sale.pn,sale.cn,sale.pn),(sale.vn),(sale.qty);

-- ###### Queries involving REGR_R2() function ###### --

SELECT DISTINCT sale.pn,sale.qty,GROUP_ID(), TO_CHAR(COALESCE(REGR_R2(floor(sale.pn),floor(sale.prc+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(floor(sale.vn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.vn-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.prc-sale.cn)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY ROLLUP((sale.qty),(sale.pn,sale.vn,sale.vn),(sale.pn,sale.cn,sale.dt),(sale.pn,sale.vn,sale.qty));

-- ###### Queries involving REGR_SLOPE() function ###### --

SELECT DISTINCT sale.cn,sale.dt,GROUP_ID(), TO_CHAR(COALESCE(REGR_SLOPE(floor(sale.qty/sale.pn),floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_SAMP(floor(sale.qty+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn-sale.prc)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY GROUPING SETS((sale.pn,sale.qty),(sale.cn)),ROLLUP((sale.prc,sale.prc)),ROLLUP((sale.pn,sale.pn),(sale.qty,sale.vn),(sale.cn,sale.pn,sale.qty),(sale.pn),(sale.cn,sale.prc),(sale.cn)),sale.dt HAVING GROUP_ID() >= 0;

-- ###### Queries involving REGR_SXX() function ###### --

SELECT DISTINCT sale.cn,sale.qty,sale.cn,sale.qty,GROUPING(sale.qty,sale.cn,sale.qty),GROUP_ID(), TO_CHAR(COALESCE(REGR_SXX(floor(sale.qty*sale.pn),floor(sale.vn/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.pn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(floor(sale.vn*sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.vn/sale.pn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY GROUPING SETS(CUBE((sale.pn),(sale.qty,sale.qty)),(),CUBE((sale.pn),(sale.vn,sale.vn),(sale.vn,sale.prc),(sale.prc),(sale.pn,sale.qty),(sale.pn,sale.qty))),ROLLUP((sale.dt),(sale.vn,sale.vn),(sale.prc,sale.cn),(sale.dt,sale.vn),(sale.dt,sale.vn),(sale.vn,sale.vn));

-- ###### Queries involving REGR_SXY() function ###### --

SELECT DISTINCT sale.cn,sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(sale.cn),floor(sale.qty/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.cn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.pn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY ROLLUP((sale.pn),(sale.pn,sale.cn),(sale.vn),(sale.pn)),GROUPING SETS((),ROLLUP((sale.prc,sale.pn),(sale.cn,sale.cn,sale.pn),(sale.vn,sale.pn)),ROLLUP((sale.pn)));

-- ###### Queries involving REGR_SYY() function ###### --

SELECT DISTINCT sale.cn,sale.cn,sale.qty,sale.dt,sale.qty,sale.cn,GROUPING(sale.cn,sale.qty,sale.qty,sale.cn,sale.dt,sale.cn),GROUP_ID(), TO_CHAR(COALESCE(REGR_SYY(floor(sale.prc),floor(sale.pn*sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(floor(sale.prc/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.qty+sale.pn)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY ROLLUP((sale.qty,sale.dt)),ROLLUP((sale.pn,sale.vn),(sale.qty,sale.cn),(sale.cn),(sale.cn,sale.qty)),(sale.vn,sale.qty);

-- ###### Queries involving AVG() function ###### --

SELECT DISTINCT sale.vn,sale.pn,sale.cn,GROUPING(sale.pn,sale.pn), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(floor(sale.qty+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(floor(sale.cn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn*sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.pn*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn/sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_POP(floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(floor(sale.qty-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_POP(floor(sale.prc-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY CUBE((sale.cn,sale.vn)),(sale.qty),(sale.cn),(sale.vn,sale.prc),(sale.qty),(sale.pn,sale.pn,sale.qty),(sale.pn),(sale.cn,sale.prc),(sale.qty);

-- ###### Queries involving COUNT() function ###### --

SELECT DISTINCT sale.cn,sale.vn,sale.qty,sale.pn, TO_CHAR(COALESCE(COUNT(floor(sale.prc-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(floor(sale.cn-sale.vn)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY CUBE((sale.qty,sale.dt),(sale.dt),(sale.vn),(sale.pn,sale.qty,sale.vn)),GROUPING SETS(CUBE((sale.cn,sale.qty),(sale.vn,sale.prc))),GROUPING SETS((),CUBE((sale.cn,sale.pn),(sale.dt,sale.dt),(sale.vn),(sale.cn,sale.prc,sale.pn)),ROLLUP((sale.cn),(sale.vn),(sale.qty),(sale.vn,sale.dt)));

-- ###### Queries involving MAX() function ###### --

SELECT DISTINCT sale.vn, TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc/sale.vn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((sale.prc),(sale.prc,sale.vn),(sale.qty,sale.qty),(sale.vn)),GROUPING SETS((),ROLLUP((sale.prc,sale.cn,sale.prc),(sale.cn,sale.cn),(sale.vn),(sale.vn,sale.cn))),GROUPING SETS(CUBE((sale.cn),(sale.pn,sale.qty,sale.qty)),ROLLUP((sale.prc,sale.cn,sale.prc),(sale.qty)));

-- ###### Rollup with DQA and constants in target list expressions and qualifications with same value as that of constant grouping column ###### --
SELECT COUNT(DISTINCT cn) as cn_r, f, g FROM (SELECT cn, CASE WHEN (vn = 0) THEN 1 END AS f, 1 AS g FROM sale) sale_view GROUP BY ROLLUP(f,g);
SELECT COUNT(DISTINCT cn) as cn_r, f, g FROM (SELECT cn, vn + 1 AS f, 1 AS g FROM sale) sale_view GROUP BY ROLLUP(f,g) HAVING (f > 1);
PREPARE p AS SELECT COUNT(DISTINCT cn) as cn_r, f, g FROM (SELECT cn, vn + $1 AS f, $1 AS g FROM sale) sale_view GROUP BY ROLLUP(f,g) HAVING (g > 1);
EXECUTE p(2);

-- ###### Queries involving CUBE with HAVING CLAUSE ###### --

WITH src AS (SELECT 1 AS a, 1 AS b)
SELECT 1 FROM src GROUP BY CUBE(a, b) HAVING a IS NOT NULL;
