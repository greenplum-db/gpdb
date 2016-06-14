
-- ----------------------------------------------------------------------
-- Test: setup_schema.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_resource_queue;
set search_path to qp_resource_queue;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop role if exists tbl16369_user1;
drop role if exists tbl16369_user3;
drop resource queue tbl16369_resq1;
drop resource queue tbl16369_resq3;
drop function if exists tbl16369_func1();
drop function if exists tbl16369_func2();
drop function if exists tbl16369_func3(refcursor, refcursor);
drop function if exists tbl16369_func4();
drop function if exists tbl16369_func5();
drop function if exists tbl16369_func6();
drop function if exists tbl16369_func7();
drop function if exists tbl16369_func8();
drop function if exists tbl16369_func9();
drop function if exists tbl16369_func10(param_numcount integer);
drop function if exists tbl16369_func11(param_numcount integer);
drop function if exists tbl16369_func12();
drop table if exists tbl16369_test;
drop table if exists tbl16369_zipcode_gis;
drop table if exists tbl16369_test1;
drop table if exists tbl16369_x;
-- end_ignore
create resource queue tbl16369_resq1 WITH (ACTIVE_STATEMENTS=1);
CREATE ROLE tbl16369_user1 LOGIN PASSWORD 'tbl16369pwd' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE tbl16369_resq1;
create resource queue tbl16369_resq3 WITH (ACTIVE_STATEMENTS=1, MEMORY_LIMIT='200MB');
CREATE ROLE tbl16369_user3 LOGIN PASSWORD 'tbl16369pwd' NOSUPERUSER INHERIT NOCREATEDB NOCREATEROLE RESOURCE QUEUE tbl16369_resq3;
-- start_ignore
drop role if exists gpadmin;
create role gpadmin login superuser;
-- end_ignore
grant gpadmin to tbl16369_user1;
grant gpadmin to tbl16369_user3;
Drop table if exists tbl16369_test;
create table tbl16369_test(col text);
CREATE TABLE tbl16369_zipcode_gis ( zipcode INTEGER, zip_col1 INTEGER, zip_col2 INTEGER ) DISTRIBUTED BY ( zipcode );
insert into tbl16369_test values ('789'),('789'),('789'),('789'),('789'),('789');
create table tbl16369_test1(a text, b int);
insert into tbl16369_test1 values ('456',456),('012',12),('901',901),('678',678),('789',789),('345',345);
insert into tbl16369_test1 values ('456',456),('012',12),('901',901),('678',678),('789',789),('345',345);
create table tbl16369_x(x int);
insert into tbl16369_x values (456),(12),(901),(678),(789),(345);
insert into tbl16369_x values (456),(12),(901),(678),(789),(345);
INSERT INTO tbl16369_zipcode_gis VALUES ( 94403, 123, 123 );
INSERT INTO tbl16369_zipcode_gis VALUES ( 94404, 321, 321 );
INSERT INTO tbl16369_zipcode_gis VALUES ( 90405, 234, 234 );
CREATE FUNCTION tbl16369_func1() RETURNS tbl16369_test AS $$
DECLARE
         res tbl16369_test;
BEGIN
         select * into res from tbl16369_test;
         return res;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func2() RETURNS tbl16369_test AS $$
DECLARE
         rec tbl16369_test;
         ref refcursor;
BEGIN
         OPEN ref FOR SELECT * FROM tbl16369_test;
         FETCH ref into rec;
         CLOSE ref;
         RETURN rec;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func3(refcursor, refcursor) RETURNS SETOF refcursor AS $$
BEGIN
    OPEN $1 FOR SELECT * FROM tbl16369_zipcode_gis;
    RETURN NEXT $1;
     OPEN $2 FOR SELECT * FROM tbl16369_test;
     RETURN NEXT $2;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func4() RETURNS tbl16369_test AS $$
DECLARE
        res tbl16369_test;
BEGIN
        select tbl16369_func1() into res;
        return res;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func5() RETURNS tbl16369_test AS $$
DECLARE
        res tbl16369_test;
BEGIN
        select tbl16369_func2() into res;
        return res;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func6() RETURNS setof refcursor AS $$
DECLARE
        ref1 refcursor;
BEGIN
        select tbl16369_func3('a','b') into ref1;
        return next ref1;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func7() RETURNS tbl16369_test AS $$
DECLARE
        ref refcursor;
        rec tbl16369_test;
BEGIN
     OPEN ref FOR SELECT tbl16369_func1();
     FETCH ref into rec;
     CLOSE ref;
     RETURN rec;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func8() RETURNS tbl16369_test AS $$
DECLARE
     ref refcursor;
     rec tbl16369_test;
BEGIN
     OPEN ref FOR SELECT tbl16369_func2();
     FETCH ref into rec;
     CLOSE ref;
     RETURN rec;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func9() RETURNS setof refcursor AS $$
DECLARE
     ref refcursor;
     ref1 refcursor;
BEGIN
     OPEN ref FOR SELECT tbl16369_func3('a','b');
     FETCH ref into ref1;
     CLOSE ref;
     RETURN next ref1;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE OR REPLACE FUNCTION tbl16369_func10(param_numcount integer)
RETURNS tbl16369_test AS $$
DECLARE res tbl16369_test;
BEGIN
   IF param_numcount < 0 THEN
       RAISE EXCEPTION 'Negative numbers are not allowed';
   ELSIF param_numcount > 0 THEN
      RAISE NOTICE 'Yo there I''m number %, next: %', param_numcount, param_numcount -1;
      res:= tbl16369_func10(param_numcount - 1);
   ELSE
      RAISE INFO 'Alas we are at the end of our journey';
      select * into res from tbl16369_test;
   END IF;
      RETURN res;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE OR REPLACE FUNCTION tbl16369_func11(param_numcount integer) RETURNS tbl16369_test AS $$
DECLARE res tbl16369_test; ref refcursor;
BEGIN
    IF param_numcount < 0 THEN
         RAISE EXCEPTION 'Negative numbers are not allowed';
    ELSIF param_numcount > 0 THEN
         RAISE NOTICE 'Yo there I''m number %, next: %', param_numcount, param_numcount -1;
         res := tbl16369_func11(param_numcount - 1);
    ELSE
         RAISE INFO 'Alas we are at the end of our journey';
         open ref for select * from tbl16369_test;
         FETCH ref into res;
         CLOSE ref;
    END IF;
    RETURN res;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE FUNCTION tbl16369_func12() RETURNS float AS $$
DECLARE
    ref refcursor;
    res float;
BEGIN
    OPEN ref FOR select avg(foo.b) from (select tbl16369_test1.b from tbl16369_test,tbl16369_test1 where tbl16369_test.col=tbl16369_test1.a and tbl16369_test1.b in (select x from tbl16369_x)) as foo;
    FETCH ref into res;
    CLOSE ref;
    RETURN res;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
Drop table if exists zipcode_gis;
Drop table if exists x;
Drop table if exists test1;
Drop function complexquery();
Drop table if exists test;
create table test(col text);
CREATE TABLE zipcode_gis ( zipcode INTEGER, zip_col1 INTEGER, zip_col2 INTEGER ) DISTRIBUTED BY ( zipcode );
insert into test values ('456'),('789'),('012'),('345'),('678'),('901');
create table test1(a text, b int);
insert into test1 values ('456',456),('012',12),('901',901),('678',678),('789',789),('345',345);
insert into test1 values ('654',456),('210',12),('109',901),('876',678),('987',789),('543',345);
create table x(x int);
insert into x values (456),(12),(901),(678),(789),(345);
insert into x values (456),(12),(901),(678),(789),(345);
select avg(foo.b) from (select test1.b from test,test1 where test.col=test1.a and test1.b in (select x from x)) as foo;
CREATE FUNCTION complexquery() RETURNS float AS $$
DECLARE
        ref refcursor;
	res float;
BEGIN
    OPEN ref FOR select avg(foo.b) from (select test1.b from test,test1 where test.col=test1.a and test1.b in (select x from x)) as foo;
    FETCH ref into res;
    CLOSE ref;
    RETURN res;
END;
$$ LANGUAGE plpgsql;


-- ----------------------------------------------------------------------
-- Test: test01_simple_function.sql
-- ----------------------------------------------------------------------

select tbl16369_func1();

-- ----------------------------------------------------------------------
-- Test: test02_fn_with_simple_cursor.sql
-- ----------------------------------------------------------------------

select tbl16369_func2();

-- ----------------------------------------------------------------------
-- Test: test03_fn_with_cursor_as_arg_rtntype.sql
-- ----------------------------------------------------------------------

select tbl16369_func3('a','b');

-- ----------------------------------------------------------------------
-- Test: test04_nested_simple_qry_calls_simple_qry.sql
-- ----------------------------------------------------------------------

select tbl16369_func4();

-- ----------------------------------------------------------------------
-- Test: test05_nested_simple_qry_calls_cursor.sql
-- ----------------------------------------------------------------------

select tbl16369_func5();

-- ----------------------------------------------------------------------
-- Test: test06_nested_simple_qry_calls_cursor_as_arg_rtntype.sql
-- ----------------------------------------------------------------------

select tbl16369_func6();

-- ----------------------------------------------------------------------
-- Test: test07_nested_cursor_calls_simple_query_tbl16369_user1.sql
-- ----------------------------------------------------------------------

select tbl16369_func7();

-- ----------------------------------------------------------------------
-- Test: test07_nested_cursor_calls_simple_query_tbl16369_user3.sql
-- ----------------------------------------------------------------------

select tbl16369_func7();

-- ----------------------------------------------------------------------
-- Test: test08_nested_cursor_calls_cursor_tbl16369_user1.sql
-- ----------------------------------------------------------------------

select tbl16369_func8();

-- ----------------------------------------------------------------------
-- Test: test08_nested_cursor_calls_cursor_tbl16369_user3.sql
-- ----------------------------------------------------------------------

select tbl16369_func8();

-- ----------------------------------------------------------------------
-- Test: test09_nested_cursor_calls_cursor_as_arg_rtntype_tbl16369_user1.sql
-- ----------------------------------------------------------------------

select tbl16369_func9();

-- ----------------------------------------------------------------------
-- Test: test09_nested_cursor_calls_cursor_as_arg_rtntype_tbl16369_user3.sql
-- ----------------------------------------------------------------------

select tbl16369_func9();

-- ----------------------------------------------------------------------
-- Test: test10_recursive_plpgsql_functions.sql
-- ----------------------------------------------------------------------

select tbl16369_func10(5);

-- ----------------------------------------------------------------------
-- Test: test11_recursive_plpgsql_functions_with_curosr.sql
-- ----------------------------------------------------------------------

select tbl16369_func11(5);

-- ----------------------------------------------------------------------
-- Test: test12_funtion_with_complex_query.sql
-- ----------------------------------------------------------------------

select avg(foo.b) from (select test1.b from test,test1 where test.col=test1.a and test1.b in (select x from x)) as foo;

select complexquery();

-- ----------------------------------------------------------------------
-- Test: test13_funtion_with_complex_query_in_cursor.sql
-- ----------------------------------------------------------------------

select tbl16369_func12();

-- ----------------------------------------------------------------------
-- Test: test14_multiple_function_calls.sql
-- ----------------------------------------------------------------------

select tbl16369_func1(),tbl16369_func2(),tbl16369_func4(),tbl16369_func5(),tbl16369_func10(5), tbl16369_func11(5), tbl16369_func12(), generate_series(1,10);

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_resource_queue cascade;
drop role if exists tbl16369_user1;
drop role if exists tbl16369_user3;
drop resource queue tbl16369_resq1;
drop resource queue tbl16369_resq3;
-- end_ignore
