drop function add_em(integer,integer);
drop function add_em(integer,integer,integer);
drop function add_num(x integer, y integer, OUT sum integer, OUT product integer);
drop type sum_prod cascade;

drop table bank_ac;
drop table emp_fun cascade;
drop table tab_sour cascade;
drop table fun_tree;
drop table logtable;
drop table db;

drop table stress_source;
drop table stress_table;

drop aggregate agg_point_add1(point);
drop table agg_point_tbl;
drop aggregate agg_point_add2(point);
drop table agg_numeric_tbl;
drop table if exists test;
create table test (a integer, b integer);
insert into test select a, a%25 from generate_series(1,100) a;
select greatest(a,b) from test;
select name, increment(emp_fun) as new_sal from emp_fun where emp_fun.name='bill';
create or replace function set_tab(int) returns SETOF tab_sour as $$ select * from tab_sour where tabid=$1; $$ language sql READS SQL DATA;
select * from set_tab(1) as new_tab;
SELECT round(4, 0);
CREATE or REPLACE FUNCTION sales_tax(subtotal real, OUT tax real) AS $$
                                                BEGIN
                                                    tax := subtotal * 0.06;
                                                END;
                                                $$ LANGUAGE plpgsql NO SQL;
select sales_tax(30);

-- --------------------------------------------------------------------------------------
-- COPYRIGHT: Copyright (c) 2010, Greenplum.  All rights reserved.
-- WARNINGS (if applicable):
-- ALGORITHM (optional):
--
-- AUTHOR: Ngoc Lam-Miller
-- PURPOSE: bug: MPP-7879 - query94 causes SIGSEGV
--      - It might be the well known pl/pgsql catching OIDs issue 
--		This is a fix for this possible cause.
--	- I added this test to compare with query94
-- LAST MODIFIED:
--      - 2010/01/29: initial version
-- --------------------------------------------------------------------------------------




-- start_ignore
drop function bad_ddl();
-- end_ignore

create function bad_ddl() returns void as $body$
        begin
                 execute 'create table junk_table(a int)';
                 execute 'drop table junk_table';
         end;
$body$ language plpgsql volatile MODIFIES SQL DATA;

select bad_ddl();

-- start_ignore
drop function bad_ddl();
-- end_ignore
