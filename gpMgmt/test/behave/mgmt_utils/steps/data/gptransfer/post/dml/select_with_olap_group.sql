\c gptest;

SELECT sale.pn,sale.qty,sale.prc,sale.prc, TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.pn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(floor(sale.prc+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VAR_SAMP(floor(sale.qty+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.pn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY CUBE((sale.vn,sale.pn),(sale.cn,sale.vn),(sale.cn,sale.cn),(sale.qty,sale.dt)),ROLLUP((sale.qty,sale.vn,sale.cn),(sale.dt,sale.dt)),sale.prc;
