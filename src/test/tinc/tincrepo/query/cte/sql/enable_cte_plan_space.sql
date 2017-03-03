-- @author prabhd
-- @created 2014-02-25 12:00:00
-- @modified 2014-02-25 12:00:00
-- @tags cte HAWQ
-- @checkplan True
-- @optimizer_mode on
-- @product_version gpdb: [4.3.1-], hawq: [1.2.1-]
-- @gucs optimizer=on;optimizer_enumerate_plans=on;client_min_messages='log'
-- @description MPP-22085 Enabling CTE inlining reduces plan space

-- start_ignore
set optimizer_cte_inlining=off;
explain with v as (select x,y from bar) select v1.x from v v1, v v2 where v1.x = v2.x;

set optimizer_cte_inlining=on;
set optimizer_cte_inlining_bound=1000;
explain with v as (select x,y from bar) select v1.x from v v1, v v2 where v1.x = v2.x;
--end_ignore

