SET datestyle = "ISO, DMY";

-- Test heuristic to resolve unknown-type literals when there are ambiguous
-- functions, added in PostgreSQL commit 1a8b9fb549.
create function ambigargfunc(a int4, b int4) returns text as $$ select 'int variant chosen'::text $$ language sql;
create function ambigargfunc(a int4, b timestamp) returns text as $$ select 'timestamp variant chosen'::text $$ language sql;

select ambigargfunc(1,'2');

-- start_ignore
drop table if exists test;
create table test (a integer, b integer);

insert into test select a, a%25 from generate_series(1,100) a;
-- end_ignore

select greatest(a,b) from test order by a;
select least(a,b) from test order by a;

-- start_ignore
CREATE OR REPLACE FUNCTION add_em(integer, integer) RETURNS integer AS $$ SELECT $1 + $2; $$ LANGUAGE SQL;
-- end_ignore

SELECT add_em(75, 25) AS answer;
SELECT add_em(25, 75, 100) AS answer;

-- start_ignore
CREATE or replace FUNCTION add_em(integer, integer) RETURNS integer AS $$ SELECT $12 + $20; $$ LANGUAGE SQL;
CREATE or replace FUNCTION add_em(integer, integer) RETURNS integer AS $$ SELECT $1 + $2 + $3; $$ LANGUAGE SQL;
CREATE or replace FUNCTION add_em(integer, integer, integer) RETURNS integer AS $$ SELECT $1 + $2; $$ LANGUAGE SQL;
-- end_ignore


SELECT add_em(25, 75, 100) AS answer;
SELECT add_em(25, 75) AS answer;
SELECT add_em(25) AS answer;

-- start_ignore
create table bank_ac(acno int, name char(20), balance numeric);

insert into bank_ac values(1,'anne',1000);
insert into bank_ac values(3,'chinni',3000);
insert into bank_ac values(4,'dany',4000);
insert into bank_ac values(5,'eric',5000);
insert into bank_ac values(6,'frank',6000);
-- end_ignore

\echo -- order 1
-- order 1
select * from bank_ac order by acno;

-- start_ignore
create or replace function ac_debit1(int,numeric) returns integer AS ' update bank_ac set balance = balance - $2 where acno=$1; select 1;' language sql;
-- end_ignore

select ac_debit1(2,100);

-- start_ignore
create or replace function ac_debit(int,numeric) returns numeric as 'UPDATE bank_ac set balance = balance - $2 where acno = $1; select balance from bank_ac where acno = $1;' language sql;
-- end_ignore

select ac_debit(4, 500);
select * from bank_ac order by acno;

-- start_ignore
create or replace function ac_credit(int,numeric) returns numeric as 'UPDATE bank_ac set balance = balance + $2 where acno = $1; select balance from bank_ac where acno = $1;' language sql;
-- end_ignore

select ac_credit(2, 800);

-- start_ignore
create or replace function ac_delete(int) returns int AS 'delete from bank_ac where acno=$1;' language sql;
create or replace function ac_delete(int) returns int AS 'delete from bank_ac where acno=$1; select 1;' language sql;
-- end_ignore

select ac_delete(1);

-- start_ignore
insert into bank_ac values(1,'Angle',1000);

create table emp_fun(name text, salary numeric, age integer, cubicle point);


insert into emp_fun values ( 'apple',1000,23,'(1,1)');
insert into emp_fun values ( 'bill',2000,25,'(1,2)');
insert into emp_fun values ( 'cathy',3000,22,'(2,1)');
insert into emp_fun values ( 'drill',3400,45,'(2,2)');


create or replace function increment(emp_fun) returns numeric as ' select $1.salary*2 as salary; ' language sql;
-- end_ignore

select * from emp_fun order by name;
select name, increment(emp_fun) as new_sal from emp_fun where emp_fun.name='bill';
select name, increment(emp_fun.*) AS new_sal from emp_fun where emp_fun.cubicle ~= point '(2,1)';
select name, increment(ROW(name,salary*1.1,age,cubicle)) AS new_sal from emp_fun order by name;

-- start_ignore
create or replace function new_emp_fun() returns emp_fun as $$ select text 'henry' AS name, 1000.0 AS salary, 25 AS age, point '(2,2)' AS cubicle; $$ language sql;
-- end_ignore

select new_emp_fun();

-- start_ignore
create or replace function new_emp_fun() returns emp_fun AS $$ select ROW('henry',1000.0,25,'(2,2)')::emp_fun; $$ language sql;
-- end_ignore

select new_emp_fun();
select * from new_emp_fun();
select (new_emp_fun()).name;
select new_emp_fun().name;
select name(new_emp_fun());

-- start_ignore
create or replace function get_emp_name(emp_fun) returns text AS $$ select $1.name; $$language sql;
-- end_ignore

select get_emp_name(new_emp_fun());

-- start_ignore
create or replace function add_num2(IN x int, IN y int, OUT sum int) AS 'select $1+$2' language sql;
-- end_ignore

select add_num2(25,35);

-- start_ignore
create or replace function add_num2(IN x int, IN y int) AS 'select $1+$2' language sql;
create or replace function add_num(IN x int, IN y int, OUT sum int, OUT product int) AS 'select $1+$2, $1*$2' language sql;
-- end_ignore

select add_num(4,5);

-- start_ignore
create or replace function add_num(x int, y int, sum int, product int) returns integer AS 'select $1+$2, $1*$2;' language sql;
create type sum_prod AS (sum int, product int);
create or replace function num_add_prod(int,int) returns sum_prod AS 'select $1 + $2, $1*$2' language sql;
-- end_ignore

select num_add_prod(10,12);

-- start_ignore
create or replace function add_inout(IN x int, INOUT y int) AS 'select $1+$2' language sql;
-- end_ignore

select add_inout(4,8);

-- start_ignore
create table tab_sour(tabid int, tabsubid int, tabname text);

insert into tab_sour values(1,1,'joe');
insert into tab_sour values(1,2,'ed');
insert into tab_sour values(2,1,'mary');
-- end_ignore

select * from tab_sour;

-- start_ignore
create or replace function get_tab(int) returns tab_sour AS $$ select * from tab_sour where tabid=$1; $$ language sql;
-- end_ignore

select *, upper(tabname) from get_tab(1) as t1;

-- start_ignore
create or replace function set_tab(int) returns SETOF tab_sour as $$ select * from tab_sour where tabid=$1; $$ language sql;
-- end_ignore

select * from set_tab(1) as new_tab;

-- start_ignore
create table fun_tree( name text, parent text);

insert into fun_tree values('grand_parent','');
insert into fun_tree values('parent1','grand_parent');
insert into fun_tree values('parent2','grand_parent');
insert into fun_tree values('parent3','grand_parent');
insert into fun_tree values('child1','parent1');
insert into fun_tree values('child2','parent1');
insert into fun_tree values('child3','parent2');
insert into fun_tree values('child4','parent3');
-- end_ignore

select * from fun_tree;

-- start_ignore
create or replace function child_list(text) returns SETOF text AS $$ select name FROM fun_tree where parent =$1 order by name $$ language sql;
-- end_ignore

select child_list('grand_parent');
select name, child_list(name) from fun_tree;

-- start_ignore
create or replace function fun_array(anyelement,anyelement) returns anyarray AS $$ select array[$1,$2]; $$ language sql;
-- end_ignore

select fun_array(25,50) as intarray, fun_array('abc'::text, 'bcf') AS textarray;
select fun_array(25,50) as intarray, fun_array('abc', 'bcf') AS textarray;

-- start_ignore
create or replace function fun_is_greater(anyelement,anyelement) returns boolean AS $$ select $1 > $2 $$ language sql;
-- end_ignore

select fun_is_greater(3,5);
select fun_is_greater(7,5);

-- start_ignore
create or replace function invalid_func() returns anyelement as $$ select 1; $$ language sql;
create or replace function fun_dup(f1 anyelement, OUT f2 anyelement, OUT f3 anyarray) AS 'select $1, array[$1,$1]' language sql;
-- end_ignore

select * from fun_dup(22);
select current_database();
select current_schema();
select current_schemas(true);
select current_schemas(false);
select current_setting('datestyle');
SELECT round(4, 2);
SELECT round(4, 0);
SELECT round(4, -1);
SELECT round(4);
SELECT round(4, 1+1);
SELECT round(CAST (4 AS numeric), 4);
SELECT round(4.0, 4);
SELECT substr('1234', 3);
SELECT substr(varchar '1234', 3);
SELECT substr(CAST (varchar '1234' AS text), 3);
SELECT substr(CAST (varchar '1234' AS text), 3);
SELECT substr(1234, 3);
SELECT substr(CAST (1234 AS text), 3);
select * from generate_series(2,4);
select * from generate_series(5,1,-2);
select * from generate_series(4,3);
select  date '2007-12-11' + s.a as dates from generate_series(0,14,7) as s(a);

-- start_ignore
CREATE LANGUAGE plpgsql;
CREATE or REPLACE FUNCTION add_one (integer) RETURNS integer AS '
	BEGIN
		RETURN $1 + 1;
	END;	
' LANGUAGE 'plpgsql';
-- end_ignore

select add_one(4);

-- start_ignore
CREATE or REPLACE FUNCTION concat_text (text, text) RETURNS text AS '
	BEGIN
		RETURN $1 || $2;
	END;
' LANGUAGE 'plpgsql';
--end_ignore

select concat_text('Green','plum');

-- start_ignore
CREATE or REPLACE FUNCTION sales_tax(subtotal real) RETURNS real AS $$
    BEGIN
      RETURN subtotal * 0.06;
    END;
$$ LANGUAGE plpgsql;
-- end_ignore

select sales_tax(30);

-- start_ignore
CREATE or REPLACE FUNCTION sales_tax_new(real) RETURNS real AS $$
      DECLARE
             subtotal ALIAS FOR $1;
      BEGIN
             RETURN subtotal * 0.06;
      END;
$$ LANGUAGE plpgsql;
-- end_ignore

select sales_tax_new(40.68);

-- start_ignore
CREATE or REPLACE FUNCTION instr(varchar, integer) RETURNS integer AS $$
                                                DECLARE
                                                    v_string ALIAS FOR $1;
                                                    index ALIAS FOR $2;
                                                BEGIN
                                                        RETURN index+10;
                                                END;
-- end_ignore

$$ LANGUAGE plpgsql;

select instr('next',3);

-- start_ignore
CREATE or REPLACE FUNCTION instr(varchar, integer) RETURNS integer AS $$
                                                DECLARE
                                                    v_string ALIAS FOR $1;
                                                    index ALIAS FOR $2;
                                                BEGIN
                                                        RETURN index*10;
                                                END;
-- end_ignore

$$ LANGUAGE plpgsql;

-- start_ignore
CREATE or REPLACE FUNCTION sales_tax(subtotal real, OUT tax real) AS $$
                                                BEGIN
                                                    tax := subtotal * 0.06;
                                                END;
                                                $$ LANGUAGE plpgsql;
-- end_ignore

select sales_tax(30);

-- start_ignore
CREATE or REPLACE FUNCTION sum_n_product(x int, y int, OUT sum int, OUT prod int) AS $$
                                                BEGIN
                                                    sum := x + y;
                                                    prod := x * y;
                                                END;
                                        $$ LANGUAGE plpgsql;

-- end_ignore

select sum_n_product(12,10);

-- start_ignore
CREATE or REPLACE FUNCTION add_three_values(v1 anyelement, v2 anyelement, v3 anyelement)
                                        RETURNS anyelement AS $$
                                                DECLARE
                                                    result ALIAS FOR $0;
                                                BEGIN
                                                    result := v1 + v2 + v3;
                                                    RETURN result;
                                                END;
                                        $$ LANGUAGE plpgsql;
-- end_ignore

select add_three_values(1,2,3);

-- start_ignore
CREATE or REPLACE FUNCTION add_three_values(v1 anyelement, v2 anyelement, v3 anyelement,
                                        OUT sum anyelement)
                                                AS $$
                                                BEGIN
                                                    sum := v1 + v2 + v3;
                                                END;
                                        $$ LANGUAGE plpgsql;
-- end_ignore

select add_three_values(1,2,3);

-- start_ignore
create table logtable(a text,b text);
CREATE or REPLACE FUNCTION logfunc1(logtxt text) RETURNS timestamp AS $$
                                            BEGIN
                                                INSERT INTO logtable VALUES (logtxt, '2007-12-12 10:00:00.000000');
                                                    RETURN '2007-12-12 10:00:00.000000';
                                            END;
                                        $$ LANGUAGE plpgsql;
-- end_ignore


select logfunc1('firstrow');


-- start_ignore
create table logtable(a text,b text);
-- end_ignore

select * from logtable;

-- start_ignore
CREATE or REPLACE FUNCTION logfunc2(logtxt text) RETURNS timestamp AS $$
                                            DECLARE
                                                curtime timestamp;
                                            BEGIN
                                                curtime := '2007-12-12 10:00:00.000000';
                                                INSERT INTO logtable VALUES (logtxt, curtime);
                                                RETURN curtime;
                                            END;
                                        $$ LANGUAGE plpgsql;
-- end_ignore

select logfunc2('gpdb');

-- start_ignore
CREATE TABLE db (a INT PRIMARY KEY, b TEXT);
CREATE or REPLACE FUNCTION merge_db(key INT, data TEXT) RETURNS VOID AS
                                                $$
                                                BEGIN
                                                    LOOP
                                                        UPDATE db SET b = data WHERE a = key;
                                                        IF found THEN
                                                              RETURN;
                                                        END IF;

                                                     BEGIN
                                                            INSERT INTO db(a,b) VALUES (key, data);
                                                                    RETURN;
                                                        EXCEPTION WHEN unique_violation THEN
                                                        END;
                                                    END LOOP;
                                                END;
                                        $$ LANGUAGE plpgsql;
-- end_ignore

SELECT merge_db(1, 'david');
SELECT merge_db(1, 'dennis');

-- start_ignore
CREATE OR REPLACE FUNCTION fib_fun ( num integer ) RETURNS integer AS
                                                $$
                                                 BEGIN
                                                        IF
                                                                num < 2 THEN RETURN num;
                                                        END IF;
                                                         RETURN fib_fun(num - 2) + fib_fun(num - 1);
                                                 END;
                                         $$ LANGUAGE plpgsql;
                                         
-- end_ignore

                                     
select fib_fun(8);

-- start_ignore
CREATE OR REPLACE FUNCTION fib_fast(fib_for integer) RETURNS integer AS
                                                 $$ DECLARE
                                                         ret integer := 0; nxt integer := 1; tmp integer;
                                                         BEGIN
                                                                FOR num IN 1..fib_for
                                                                        LOOP tmp := ret; ret := nxt; nxt := tmp + nxt;
                                                                 END LOOP; RETURN ret;
                                                          END; $$
                                         LANGUAGE plpgsql;
-- end_ignore

select fib_fast(4);

-- start_ignore
CREATE OR REPLACE FUNCTION set_int (lim INTEGER)
                                                          RETURNS SETOF INTEGER AS $$
                                                          BEGIN
                                                                  FOR x in 1..lim LOOP
                                                                          RETURN NEXT x;
                                                                  END LOOP;
                                                                  RETURN;
                                                          END;
                                                          $$ LANGUAGE plpgsql;
-- end_ignore

SELECT * FROM set_int(10);

-- start_ignore
CREATE OR REPLACE FUNCTION set_tab1()
                                                          RETURNS SETOF INTEGER AS $$
                                                          DECLARE
                                                                  tab_rec RECORD;
                                                          BEGIN
                                                                  FOR tab_rec IN SELECT * FROM tab_sour LOOP
                                                                          RETURN NEXT tab_rec.tabid;
                                                                  END LOOP;
                                                                  RETURN;
                                                          END;
                                                          $$ LANGUAGE plpgsql;
-- end_ignore

SELECT * FROM set_tab1();

-- start_ignore
CREATE OR REPLACE FUNCTION set_tab2()
                                                          RETURNS SETOF tab_sour AS $$
                                                          DECLARE
                                                                  tab_rec RECORD;
                                                          BEGIN
                                                                  FOR tab_rec IN SELECT * FROM tab_sour LOOP
                                                                          RETURN NEXT tab_rec;
                                                                  END LOOP;
                                                                  RETURN;
                                                          END;
                                                          $$ LANGUAGE plpgsql;
-- end_ignore

SELECT * FROM set_tab2();

-- start_ignore
CREATE OR REPLACE FUNCTION ref_tab1(tab_cur refcursor)
                                                          RETURNS refcursor AS $$
                                                          BEGIN
                                                                  OPEN tab_cur FOR SELECT * FROM tab_sour;
                                                                  RETURN tab_cur;
                                                          END;
                                                          $$ LANGUAGE plpgsql;


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
create table stress_source as select a from generate_series(1,12000) a;
create table stress_table (a int primary key, b int);
create or replace function stress_test() returns text as
                                        $body$
                                                declare
                                                        mycounter record;
                                                begin
                                                        truncate stress_table;
                                                for mycounter in
                                                        select a from stress_source order by 1 loop
                                                        insert into stress_table values(mycounter.a, mycounter.a * 10000);
                                                end loop;
                                                for mycounter in
                                                        select a from stress_source order by 1 loop
                                                        update stress_table set b = b + mycounter.a where a = mycounter.a;
                                                end loop;

                                                        return 'ok';
                                                end;
                                        $body$
                                                language plpgsql volatile strict;
-- end_ignore

select stress_test();
select stress_test();

-- start_ignore
create function bad_ddl() returns void as $body$
                                        begin
                                                create table junk_table(a int);
                                                drop table junk_table;
                                        end;
                                $body$ language plpgsql volatile;
-- end_ignore

select bad_ddl();
select bad_ddl();


-- start_ignore
drop function bad_ddl();


create function bad_ddl() returns void as $body$
        begin
                 execute 'create table junk_table(a int)';
                 execute 'drop table junk_table';
         end;
-- end_ignore

$body$ language plpgsql volatile;

select bad_ddl();

-- start_ignore
drop function bad_ddl();
-- end_ignore
SET check_function_bodies TO OFF;

-- start_ignore
CREATE FUNCTION bad_syntax_func() RETURNS INTEGER AS '
                                BEGIN
                                RTRN 0;
                                END;' LANGUAGE 'plpgsql';
                                
-- end_ignore
                                
SELECT bad_syntax_func();
SELECT * from bad_syntax_func();

-- start_ignore
DROP FUNCTION bad_syntax_func();
RESET check_function_bodies;
CREATE FUNCTION bad_syntax_func() RETURNS INTEGER AS '
                                BEGIN
                                RTRN 0;
                                END; ' LANGUAGE 'plpgsql';
CREATE AGGREGATE agg_point_add1(
                                basetype=point,
                                SFUNC=point_add,
                                STYPE=point
                        );
create table agg_point_tbl (p point);

insert into agg_point_tbl values (POINT(3,5));
insert into agg_point_tbl values (POINT(30,50));
-- end_ignore

select ' ' as "Expect (33,55)", agg_point_add1(p) from agg_point_tbl;

-- start_ignore
CREATE AGGREGATE agg_point_add2(
                                basetype=point,
                                SFUNC=point_add,
                                STYPE=point,
                                INITCOND='(25, 47)'
                        );
                        
-- end_ignore
                     
select '' as "Expect (58,102)", agg_point_add2(p)  from agg_point_tbl;

-- start_ignore

create aggregate numeric_sum(
                                basetype=numeric,
                                SFUNC=numeric_add,
                                STYPE=numeric
                        );
create table agg_numeric_tbl(num numeric);

insert into agg_numeric_tbl VALUES(30);
insert into agg_numeric_tbl VALUES(-20);

-- end_ignore

select '' as ten, numeric_sum(num) from agg_numeric_tbl;

-- start_ignore
drop aggregate numeric_sum(numeric);
create aggregate numeric_sum_plus_fifty(
                                basetype=numeric,
                                SFUNC=numeric_add,
                                STYPE=numeric,
                                INITCOND='50'
                        );
                        
-- end_ignore
                        
select '' as sixty, numeric_sum_plus_fifty(num) from agg_numeric_tbl;

drop aggregate numeric_sum_plus_fifty(char);
drop aggregate numeric_sum_plus_fifty(numeric);
drop table stress_source;
drop table stress_table;

drop aggregate agg_point_add1(point);
drop table agg_point_tbl;
drop aggregate agg_point_add2(point);
drop table agg_numeric_tbl;
