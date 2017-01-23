-- @author tungs1
-- @modified 2013-07-28 12:00:00
-- @created 2013-07-28 12:00:00
-- @description groupingfunction groupingfunc67.sql
-- @db_name groupingfunction
-- @executemode normal
-- @tags groupingfunction
-- order 1
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
