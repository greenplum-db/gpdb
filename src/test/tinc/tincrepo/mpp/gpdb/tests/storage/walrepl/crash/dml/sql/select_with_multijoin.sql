-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
select * from ( select 'a' as a) x join (select 'a' as b) y on a=b;
select * from ( ( select 'a' as a ) xx join (select 'a' as b) yy on a = b ) x join (select 'a' as c) y on a=c;
select x.b from ( ( select 'a' as a ) xx join (select 'a' as b) yy on a = b ) x join (select 'a' as c) y on a=c;
