-- @author onosen
-- @created 2014-10-29 14:00:00
-- @tags ORCA
-- @gpopt 1.506
-- @description test alter table statements that generate a CTAS

select * from pg_attribute_r11_1 order by attname;
select * from r11_1 order by 1,2,3;

select * from pg_attribute_r11_2 order by attname;
select * from r11_2 order by 1,2,3;
select attrnums from gp_distribution_policy where localoid='r11_2'::regclass;

\!grep Planner %MYD%/output_%USED_OPT%/create_table_as_11_alter_table_setup.out
