-- @author garcic12
-- @created 2013-11-20 12:00:00
-- @modified 2013-11-20 12:00:00
-- @tags ci
-- @description CTE test with values.
with cte(foo) as ( values(42) ) values((select foo from cte));
