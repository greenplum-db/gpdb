--start_ignore
drop table if exists sirv_test1_result1;
drop table if exists sirv_test1_result2;
--end_ignore

CREATE or replace FUNCTION sirv_test1() RETURNS TEXT AS $$
declare
        result1 TEXT;
        result2 TEXT;
        result3 TEXT;
begin
        CREATE TABLE test_data1 (
                x INT
                , y INT
        ) distributed by (x);


        CREATE TABLE test_data2 (
                x INT
                , y VARCHAR
        ) distributed by(x);


        EXECUTE 'INSERT INTO test_data1 VALUES (1,1)';
        EXECUTE 'INSERT INTO test_data1 VALUES (1,2)';
        EXECUTE 'INSERT INTO test_data1 VALUES (2,3)';
        EXECUTE 'INSERT INTO test_data1 VALUES (3,4)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,1)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,2)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,3)';
        EXECUTE 'INSERT INTO test_data1 VALUES (5,2)';

        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''ONE'')';
	EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''ONE'')';
	EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''ONE'')';
	EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''ONE'')';
	
	EXECUTE 'SELECT CASE WHEN count(*)>0 then ''PASS'' ELSE ''FAIL'' END FROM test_data1' into result1;

	EXECUTE 'SELECT CASE WHEN count(*)>0 then ''PASS'' ELSE ''FAIL'' END FROM test_data2' into result2;

	EXECUTE 'DROP TABLE test_data1';
	EXECUTE 'DROP TABLE test_data2';

	IF (result1 = 'PASS')  and  (result2 = 'PASS') THEN
	   result3 = 'PASS';
	else
	   result3 = 'FAIL';

	END IF;

	return result3;

end $$ language plpgsql volatile MODIFIES SQL DATA;

--ctas with sirv in the select list
--start_ignore
explain create table sirv_test1_result1 as select sirv_test1() as res distributed by (res);
--end_ignore

--ctas with sirv in select list
create table sirv_test1_result1 as select sirv_test1() as res distributed by (res);

--workaround
create table sirv_test1_result2 as select (select sirv_test1()) as res distributed by (res);

--start_equiv
select * from sirv_test1_result1;

select * from sirv_test1_result2;
--end_equiv

--start_ignore
drop table if exists countries_results;
drop table if exists sirv_test2_result1;
drop table if exists sirv_test2_result2;
--end_ignore

CREATE OR REPLACE FUNCTION sirv_test2 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN
  
  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by(country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  
  
  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta || area_delta || gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;


--ctas with sirv in the from clause
--start_ignore
explain create table sirv_test2_result1 as select * from sirv_test2(2,1000,1000) as res distributed by(res);
--end_ignore
create table sirv_test2_result1 as select * from sirv_test2(2,1000,1000) as res distributed by(res);

--workaround
create table sirv_test2_result2 as select * from (select (select sirv_test2(2,1000,1000)) as res) as foo distributed by(res);

--start_equiv
select * from sirv_test2_result1;

select * from sirv_test2_result2;
--end_equiv

--start_ignore
drop table if exists sirv_test3_result1;
drop table if exists sirv_test3_result2;
--end_ignore


CREATE OR REPLACE FUNCTION sirv_test3 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by(country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta || area_delta || gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;


create table sirv_test3_result1(id int,country_index text) distributed by(id);
create table sirv_test3_result2(id int, country_index text) distributed by(id);


--insert with sirv in the select list
--start_ignore
explain insert into sirv_test3_result1 select 1,sirv_test3(2,1000,1000);
--end_ignore
insert into sirv_test3_result1 select 1,sirv_test3(2,1000,1000);

--workaround
insert into sirv_test3_result2 select 1,(select sirv_test3(2,1000,1000));

--start_equiv
select * from sirv_test3_result1;

select * from sirv_test3_result2;
--end_equiv

--start_ignore
drop table if exists sirv_test4_result1;
drop table if exists sirv_test4_result2;
--end_ignore

CREATE OR REPLACE FUNCTION sirv_test4(gnp_diff float8, opt integer)  RETURNS FLOAT[] AS $$
declare
        res FLOAT[];
	tmp_values FLOAT[];
	gnp_curr_val float8;
        iter INT = 0;
begin
	gnp_curr_val = gnp_diff;

        CREATE TEMP TABLE l_gnp (
                gnp_values FLOAT[]
        ) distributed by (gnp_values);

        CREATE TEMP TABLE h_gnp(
                gnp_values FLOAT[]
        ) distributed by (gnp_values);

       EXECUTE 'INSERT INTO l_gnp SELECT ARRAY(SELECT gnp FROM country where gnp < '  || gnp_diff || ' order by gnp desc)';
       LOOP
		iter = iter + 1;
		TRUNCATE table h_gnp;
		EXECUTE 'SELECT ARRAY(SELECT gnp FROM country where gnp > ' || gnp_curr_val || ' order by gnp desc LIMIT 50)'  INTO tmp_values;
		EXECUTE 'INSERT INTO h_gnp(gnp_values) VALUES(array['|| array_to_string(tmp_values,',') ||'])';		

                IF(iter > 5)THEN
			EXIT;
		ELSE 
		     gnp_curr_val = gnp_curr_val + 500;
		END IF;

	END LOOP;

			
        IF(opt > 0) THEN
	       EXECUTE 'SELECT gnp_values FROM h_gnp' INTO res;
	ELSE
	       EXECUTE 'SELECT gnp_values FROM l_gnp' INTO res;
	END IF;
	
	DROP TABLE l_gnp;
	DROP TABLE h_gnp;

	RETURN res;
end
$$ LANGUAGE plpgsql volatile MODIFIES SQL DATA;


create table sirv_test4_result1(res float[]) distributed by(res);
create table sirv_test4_result2(res float[]) distributed by(res);


--insert with sirv in the from clause
--start_ignore
explain insert into sirv_test4_result1 select * from sirv_test4(20000,0);
--end_ignore
insert into sirv_test4_result1 select * from sirv_test4(20000,0);
insert into sirv_test4_result1 select * from sirv_test4(25000,1);
insert into sirv_test4_result1 select * from sirv_test4(30000,0);
insert into sirv_test4_result1 select * from sirv_test4(35000,1);


--workaround
insert into sirv_test4_result2 select * from (select (select sirv_test4(20000,0))) AS FOO;
insert into sirv_test4_result2 select * from (select (select sirv_test4(25000,1))) AS FOO;
insert into sirv_test4_result2 select * from (select (select sirv_test4(30000,0))) AS FOO;
insert into sirv_test4_result2 select * from (select (select sirv_test4(35000,1))) AS FOO;

--start_equiv
select * from sirv_test4_result1 order by res;

select * from sirv_test4_result2 order by res;
--end_equiv

--start_ignore
drop table if exists sirv_test5_result1;
drop table if exists sirv_test5_result2;
--end_ignore

CREATE or replace FUNCTION sirv_test5_fun1() RETURNS TEXT AS $$
declare
        result1 TEXT;
        result2 TEXT;
        result3 TEXT;
begin

        EXECUTE 'CREATE TABLE test_data1 (
                x INT
                , y INT
        ) distributed by(x)';

	EXECUTE 'CREATE TABLE test_data2 (
                x INT
                , y VARCHAR
        ) distributed by(x)';


        EXECUTE 'INSERT INTO test_data1 VALUES (1,1)';
        EXECUTE 'INSERT INTO test_data1 VALUES (1,2)';
        EXECUTE 'INSERT INTO test_data1 VALUES (2,3)';
        EXECUTE 'INSERT INTO test_data1 VALUES (3,4)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,1)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,2)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,3)';
        EXECUTE 'INSERT INTO test_data1 VALUES (5,2)';

        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''ONE'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (2, ''two'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (2, ''TWO'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (3, ''three'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (7, ''seven'')';

	
	EXECUTE 'SELECT CASE WHEN count(*)>0 then ''PASS'' ELSE ''FAIL'' END FROM test_data1' INTO result1;

	EXECUTE 'SELECT CASE WHEN count(*)>0 then ''PASS'' ELSE ''FAIL'' END FROM test_data2' INTO result2;

	EXECUTE 'DROP TABLE test_data1';
	EXECUTE 'DROP TABLE test_data2';

	IF (result1 = 'PASS')  and  (result2 = 'PASS') THEN
	   result3 = 'PASS';
	else
	   result3 = 'FAIL';

	END IF;

	return result3;

end $$ language plpgsql volatile MODIFIES SQL DATA;


CREATE OR REPLACE FUNCTION sirv_test5_fun2 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta || area_delta || gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;


CREATE OR REPLACE FUNCTION sirv_test5_fun3 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS float
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by(country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta +  area_delta +  gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;


CREATE OR REPLACE FUNCTION sirv_test5_fun4(gnp_diff float8, opt integer)  RETURNS FLOAT[] AS $$
declare
        res FLOAT[];
	tmp_values FLOAT[];
	gnp_curr_val float8;
        iter INT = 0;
begin
	gnp_curr_val = gnp_diff;

        CREATE TEMP TABLE l_gnp (
                gnp_values FLOAT[]
        ) distributed by(gnp_values);

        CREATE TEMP TABLE h_gnp(
                gnp_values FLOAT[]
        ) distributed by (gnp_values);

       EXECUTE 'INSERT INTO l_gnp SELECT ARRAY(SELECT gnp FROM country where gnp < '  || gnp_diff || ' order by gnp desc)';
       LOOP
		iter = iter + 1;
		TRUNCATE table h_gnp;
		EXECUTE 'SELECT ARRAY(SELECT gnp FROM country where gnp > ' || gnp_curr_val || ' order by gnp desc LIMIT 50)'  INTO tmp_values;
		EXECUTE 'INSERT INTO h_gnp(gnp_values) VALUES(array['|| array_to_string(tmp_values,',') ||'])';		

                IF(iter > 5)THEN
			EXIT;
		ELSE 
		     gnp_curr_val = gnp_curr_val + 500;
		END IF;

	END LOOP;

			
        IF(opt > 0) THEN
	       EXECUTE 'SELECT gnp_values FROM h_gnp' INTO res;
	ELSE
	       EXECUTE 'SELECT gnp_values FROM l_gnp' INTO res;
	END IF;

	EXECUTE 'DROP table l_gnp';
        EXECUTE 'DROP table h_gnp';
	RETURN res;
end
$$ LANGUAGE plpgsql volatile MODIFIES SQL DATA;




--select list
--start_ignore
explain create table sirv_test5_result1 as select 'sirv_test1:' || sirv_test5_fun1() as field1,
                                          'sirv_test5_fun2:' || substring(sirv_test5_fun2(2,1000,1000),0,5) as field2,
                                          1.5 * sirv_test5_fun3(2,1000,1000) as field3,
                                          ARRAY[1.0::float,2.0::float] || sirv_test5_fun4(20000,0) as field4
        distributed by (field1);
--end_ignore
create table sirv_test5_result1 as select 'sirv_test1:' || sirv_test5_fun1() as field1,
                                          'sirv_test5_fun2:' || substring(sirv_test5_fun2(2,1000,1000),0,5) as field2,
                                          1.5 * sirv_test5_fun3(2,1000,1000) as field3,
                                          ARRAY[1.0::float,2.0::float] || sirv_test5_fun4(20000,0) as field4
distributed by (field1);

select * from sirv_test5_result1;


--from clause
--start_ignore
explain  create table sirv_test5_result2  as select * from sirv_test5_fun1() as field1,
                                                           substring(sirv_test5_fun2(2,1000,1000),0,5) as field2,
                                                           sirv_test5_fun3(2,1000,1000) as field3,
                                                           sirv_test5_fun4(20000,0) as field4
distributed by (field1);
--end_ignore
create table sirv_test5_result2  as select * from sirv_test5_fun1() as field1,
                                                           substring(sirv_test5_fun2(2,1000,1000),0,5) as field2,
                                                           sirv_test5_fun3(2,1000,1000) as field3,
                                                           sirv_test5_fun4(20000,0) as field4
distributed by (field1);


select * from sirv_test5_result2;




--start_ignore
drop table if exists sirv_test6_result1;
drop table if exists sirv_test6_result2;
--end_ignore

CREATE or replace FUNCTION sirv_test6_fun1() RETURNS TEXT AS $$
declare
        result1 TEXT;
        result2 TEXT;
        result3 TEXT;
begin

        EXECUTE 'CREATE TABLE test_data1 (
                x INT
                , y INT
        ) distributed by(x)';

	EXECUTE 'CREATE TABLE test_data2 (
                x INT
                , y VARCHAR
        ) distributed by(x)';


        EXECUTE 'INSERT INTO test_data1 VALUES (1,1)';
        EXECUTE 'INSERT INTO test_data1 VALUES (1,2)';
        EXECUTE 'INSERT INTO test_data1 VALUES (2,3)';
        EXECUTE 'INSERT INTO test_data1 VALUES (3,4)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,1)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,2)';
        EXECUTE 'INSERT INTO test_data1 VALUES (4,3)';
        EXECUTE 'INSERT INTO test_data1 VALUES (5,2)';

        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''ONE'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (1, ''one'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (2, ''two'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (2, ''TWO'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (3, ''three'')';
        EXECUTE 'INSERT INTO test_data2 VALUES (7, ''seven'')';

	
	EXECUTE 'SELECT CASE WHEN count(*)>0 then ''PASS'' ELSE ''FAIL'' END FROM test_data1' INTO result1;

	EXECUTE 'SELECT CASE WHEN count(*)>0 then ''PASS'' ELSE ''FAIL'' END FROM test_data2' INTO result2;

	EXECUTE 'DROP TABLE test_data1';
	EXECUTE 'DROP TABLE test_data2';

	IF (result1 = 'PASS')  and  (result2 = 'PASS') THEN
	   result3 = 'PASS';
	else
	   result3 = 'FAIL';

	END IF;

	return result3;

end $$ language plpgsql volatile MODIFIES SQL DATA;


CREATE OR REPLACE FUNCTION sirv_test6_fun2 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta || area_delta || gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;


CREATE OR REPLACE FUNCTION sirv_test6_fun3 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS float
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by(country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta +  area_delta +  gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;


CREATE OR REPLACE FUNCTION sirv_test6_fun4(gnp_diff float8, opt integer)  RETURNS FLOAT[] AS $$
declare
        res FLOAT[];
	tmp_values FLOAT[];
	gnp_curr_val float8;
        iter INT = 0;
begin
	gnp_curr_val = gnp_diff;

        CREATE TEMP TABLE l_gnp (
                gnp_values FLOAT[]
        ) distributed by(gnp_values);

        CREATE TEMP TABLE h_gnp(
                gnp_values FLOAT[]
        ) distributed by (gnp_values);

       EXECUTE 'INSERT INTO l_gnp SELECT ARRAY(SELECT gnp FROM country where gnp < '  || gnp_diff || ' order by gnp desc)';
       LOOP
		iter = iter + 1;
		TRUNCATE table h_gnp;
		EXECUTE 'SELECT ARRAY(SELECT gnp FROM country where gnp > ' || gnp_curr_val || ' order by gnp desc LIMIT 50)'  INTO tmp_values;
		EXECUTE 'INSERT INTO h_gnp(gnp_values) VALUES(array['|| array_to_string(tmp_values,',') ||'])';		

                IF(iter > 5)THEN
			EXIT;
		ELSE 
		     gnp_curr_val = gnp_curr_val + 500;
		END IF;

	END LOOP;

			
        IF(opt > 0) THEN
	       EXECUTE 'SELECT gnp_values FROM h_gnp' INTO res;
	ELSE
	       EXECUTE 'SELECT gnp_values FROM l_gnp' INTO res;
	END IF;

	EXECUTE 'DROP table l_gnp';
        EXECUTE 'DROP table h_gnp';
	RETURN res;
end
$$ LANGUAGE plpgsql volatile MODIFIES SQL DATA;



--select list
create table sirv_test6_result1 (field1 text, field2 text, field3 float, field4 float[]) distributed by (field1);
create table sirv_test6_result2 (field1 text, field2 text, field3 float, field4 float[]) distributed by (field1);

--start_ignore
explain insert into sirv_test6_result1   select 'sirv_test6_fun1:' || sirv_test6_fun1() as field1,
                                          'sirv_test6_fun2:' || substring(sirv_test6_fun2(2,1000,1000),0,5) as field2,
                                          1.5 * sirv_test6_fun3(2,1000,1000) as field3,
                                          ARRAY[1.0::float,2.0::float] || sirv_test6_fun4(20000,0);
--end_ignore

insert into sirv_test6_result1   select 'sirv_test6_fun1:' || sirv_test6_fun1() as field1,
                                          'sirv_test6_fun2:' || substring(sirv_test6_fun2(2,1000,1000),0,5) as field2,
                                          1.5 * sirv_test6_fun3(2,1000,1000) as field3,
                                          ARRAY[1.0::float,2.0::float] || sirv_test6_fun4(20000,0);


select * from sirv_test6_result1;


--from clause
--start_ignore
explain insert into  sirv_test6_result2  select * from     sirv_test6_fun1() as field1,
                                                           substring(sirv_test6_fun2(2,1000,1000),0,5) as field2,
                                                           sirv_test6_fun3(2,1000,1000) as field3,
                                                           sirv_test6_fun4(20000,0);
--end_ignore
insert into  sirv_test6_result2  select * from     sirv_test6_fun1() as field1,
                                                           substring(sirv_test6_fun2(2,1000,1000),0,5) as field2,
                                                           sirv_test6_fun3(2,1000,1000) as field3,
                                                           sirv_test6_fun4(20000,0);


select * from sirv_test6_result2;





--start_ignore
drop table if exists sirv_test9_resul1;
drop table if exists sirv_test9_result2;
drop type if exists test9_holder;
drop table if exists test9_countries cascade;
--end_ignore


CREATE  TABLE test9_countries (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code);


CREATE OR REPLACE FUNCTION sirv_test9_fun1 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS setof  test9_countries
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
  r test9_countries;
BEGIN

  for r in 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > min_languages and country.surfacearea >  min_area  and country.gnp > min_gnp order by lang_total.lang_count desc 
           ) loop
  return next r;
  end loop;

  return;
  

END;
$$
    LANGUAGE plpgsql volatile READS SQL DATA;


--ctas with srf should not apply init plan changes. this is not supported
--start_ignore
explain create table sirv_test9_result1 as select sirv_test9_fun1(2,1000,1000) as field1;

explain create table sirv_test9_result1 as select * from sirv_test9_fun1(2,1000,1000) as field1;
--end_ignore
--should fail
--create table sirv_test9_result1 as select sirv_test9_fun1(2,1000,1000);

--create table sirv_test9_result1 as select * from sirv_test9_fun1(2,1000,1000);





create type test9_holder as (lang_delta int, area_delta float8, gnp_delta float8);

CREATE OR REPLACE FUNCTION sirv_test9_fun2 (res text,min_languages integer, min_area float8, min_gnp float8)
  RETURNS setof text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
  r test9_holder%rowtype;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  for r in SELECT max(lang_count - min_languages), max(area - min_area),max( gnp -  min_gnp) FROM countries_results group by country_code order by country_code loop
  return next r.lang_delta || r.area_delta || r.gnp_delta;
  end loop;

  EXECUTE 'DROP TABLE countries_results';
  

  RETURN;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;


--ctas with srf should not apply init plan changes. this is not supported
--start_ignore
explain create table sirv_test9_result1 as select sirv_test9_fun2('result',2,1000,1000) as field1 distributed by (field1);

explain create table sirv_test9_result1 as select * from sirv_test9_fun2('result',2,1000,1000) as field1 distributed by (field1);
--end_ignore
--should fail
--create table sirv_test9_result1 as select sirv_test9_fun2('result',2,1000,1000);

--create table sirv_test9_result1 as select * from sirv_test9_fun2('result',2,1000,1000);




--start_ignore
drop table if exists sirv_test10_result1;
drop table if exists sirv_test10_result2;
drop table if exists countries cascade;
drop type if exists sirv_test10_holder;
--end_ignore


CREATE  TABLE countries (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code);


CREATE OR REPLACE FUNCTION sirv_test10_srf1 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS setof  countries
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
  r countries;
BEGIN

  for r in 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > min_languages and country.surfacearea >  min_area  and country.gnp > min_gnp order by lang_total.lang_count desc 
           ) loop
  return next r;
  end loop;

  return;
  

END;
$$
    LANGUAGE plpgsql volatile READS SQL DATA;


--insert select with srf should not apply init plan changes. this is not supported
create table sirv_test10_result1(field1 countries,country_code text,country_name text, lang_count int, area float, gnp float) distributed by (country_code);

--start_ignore
explain insert into sirv_test10_result1 select sirv_test10_srf1(2,1000,1000);

explain insert into sirv_test10_result1 select sirv_test10_srf1 from sirv_test10_srf1(2,1000,1000);
--end_ignore
--should fail
--ignore error messages
--start_ignore 
insert into sirv_test10_result1 select sirv_test10_srf1(2,1000,1000);
insert into sirv_test10_result1 select sirv_test10_srf1 from sirv_test10_srf1(2,1000,1000);
--end_ignore

select * from sirv_test10_result1; --should return 0 rows



--user defined type

create type sirv_test10_holder as (lang_delta int, area_delta float8, gnp_delta float8);

CREATE OR REPLACE FUNCTION sirv_test10_srf2 (res text,min_languages integer, min_area float8, min_gnp float8)
  RETURNS setof text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
  r sirv_test10_holder%rowtype;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  for r in SELECT max(lang_count - min_languages), max(area - min_area),max( gnp -  min_gnp) FROM countries_results group by country_code order by country_code loop
  return next r.lang_delta || r.area_delta || r.gnp_delta;
  end loop;
  
  EXECUTE 'DROP table countries_results';
  

  RETURN;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;



create table sirv_test10_result2(res text) distributed by (res);

--insert select with srf should not apply init plan changes. this is not supported
--start_ignore
explain insert into sirv_test10_result2 select sirv_test10_srf2('result',2,1000,1000);

explain insert into sirv_test10_result2 select * from sirv_test10_srf2('result',2,1000,1000);
--end_ignore
--should fail
--ignore error messages

--start_ignore
insert into sirv_test10_result2 select sirv_test10_srf2('result',2,1000,1000);

insert into sirv_test10_result2 select * from sirv_test10_srf2('result',2,1000,1000);
--end_ignore

select * from sirv_test10_result2; --should return 0 rows

--start_ignore
drop table if exists sirv_test11_input;


--end_ignore


CREATE OR REPLACE FUNCTION sirv_test11 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by(country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta || area_delta || gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;

create table sirv_test11_input as select trunc(0.5 * 5) as lang_count,surfacearea, gnp from country where continent = 'Asia' distributed by (lang_count);


--ctas with sirv taking non constant arguments. should not apply init plan changes. 
--start_ignore
explain create table sirv_test11_result1 as select sirv_test11(lang_count::int,surfacearea::float,gnp::float) as res from sirv_test11_input distributed by (res);

explain create table sirv_test11_result1 as select * from ( select sirv_test11(lang_count::int,surfacearea::float,gnp::float) as res from sirv_test11_input) FOO  distributed by (res);
--end_ignore


--start_ignore
drop table if exists sirv_test12_input;
drop table if exists sirv_test12_result1;

--end_ignore


CREATE OR REPLACE FUNCTION sirv_test12 (min_languages integer, min_area float8, min_gnp float8)
  RETURNS text
AS $$
DECLARE
  lang_delta int;
  area_delta float8;
  gnp_delta float8;
BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by(country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  
  EXECUTE 'DROP TABLE countries_results';

  RETURN lang_delta || area_delta || gnp_delta ;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;

create table sirv_test12_input as select trunc(0.5 * 5) as lang_count,surfacearea, gnp from country where continent = 'Asia' distributed by (lang_count);

create table sirv_test12_result1 (res text) distributed by (res);

--insert select with sirv taking non constant arguments. should not apply init plan changes. 
--start_ignore
explain insert into sirv_test12_result1 select sirv_test12(lang_count::int,surfacearea::float,gnp::float) from sirv_test12_input;

explain insert into sirv_test12_result1 select * from (select sirv_test12(lang_count::int,surfacearea::float,gnp::float) from sirv_test12_input) FOO;
--end_ignore

--should fail
--ignore error messages

--start_ignore
insert into sirv_test12_result1 select sirv_test12(lang_count::int,surfacearea::float,gnp::float) from sirv_test12_input;

insert into sirv_test12_result1 select * from (select sirv_test12(lang_count::int,surfacearea::float,gnp::float) from sirv_test12_input) FOO;
--end_ignore

select * from sirv_test12_result1;



--start_ignore

drop language if exists plpythonu cascade;

drop table if exists sirv_test13_result1;
drop table if exists sirv_test13_result2;

--end_ignore

CREATE LANGUAGE plpythonu;

CREATE or replace FUNCTION sirv_test13_fun1 ()
  RETURNS text
AS $$
  plpy.execute('CREATE TABLE test_data1 (x INT, y INT) distributed by (x)')
  plpy.execute('CREATE TABLE test_data2 (x INT, y VARCHAR) distributed by (x)')

  for i in range(10):
    plpy.execute('INSERT INTO test_data1 VALUES (' + str(i) + ',' + str(i) + ')')

  for i in range(10):
    plpy.execute('INSERT INTO test_data2 VALUES (' + str(i) + ',\'' + str(i) + '\')')

  result1 = plpy.execute ('SELECT CASE WHEN count(*)>0 then \'PASS\' ELSE \'FAIL\' END FROM test_data1')
  result2 = plpy.execute ('SELECT CASE WHEN count(*)>0 then \'PASS\' ELSE \'FAIL\' END FROM test_data2')

  if ((result1 is not None) & (result2 is not None)):
    result3 = 'PASS'
  else:
    result3 = 'FAIL'

  plpy.execute('DROP TABLE test_data1')
  plpy.execute('DROP TABLE test_data2')
  return result3
  
$$ LANGUAGE plpythonu MODIFIES SQL DATA;



CREATE OR REPLACE FUNCTION sirv_test13_fun2(gnp_diff float8, opt integer)  RETURNS FLOAT[] AS $$
  gnp_curr_val = gnp_diff;
  plpy.execute('CREATE TEMP TABLE l_gnp ( gnp_values FLOAT[]) distributed by (gnp_values)')
  plpy.execute('CREATE TEMP TABLE h_gnp ( gnp_values FLOAT[]) distributed by (gnp_values)')

  plpy.execute('INSERT INTO l_gnp SELECT ARRAY(SELECT gnp FROM country where gnp < ' + str(gnp_diff) + ' order by gnp desc)')
  
  gnp_curr_val = gnp_diff

  for i in range(5):
    plpy.execute('TRUNCATE table h_gnp')
    plpy.execute('INSERT INTO h_gnp SELECT ARRAY(SELECT gnp FROM country where gnp > ' +  str(gnp_curr_val) + ' order by gnp desc LIMIT 50)')
    gnp_curr_val = gnp_curr_val + 500

  if(opt > 0):
    res = plpy.execute('SELECT gnp_values from h_gnp')
  else:
    res = plpy.execute('SELECT gnp_values from l_gnp')
  
  plpy.execute('DROP TABLE l_gnp')
  plpy.execute('DROP TABLE h_gnp')
 
  return res[0]["gnp_values"]

$$ LANGUAGE plpythonu MODIFIES SQL DATA;


--select list
--start_ignore
explain create table sirv_test13_result1 as select 'sirv_test1:' || sirv_test13_fun1() as field1,
                                          ARRAY[1.0::float,2.0::float] || sirv_test13_fun2(20000,0) as field2
        distributed by (field1);
--end_ignore
create table sirv_test13_result1 as select 'sirv_test1:' || sirv_test13_fun1() as field1,
                                          ARRAY[1.0::float,2.0::float] || sirv_test13_fun2(20000,0) as field2
distributed by (field1);

select * from sirv_test13_result1;


--from clause
--start_ignore
explain  create table sirv_test13_result2  as select * from sirv_test13_fun1() as field1,
                                                            sirv_test13_fun2(20000,0) as field2
distributed by (field1);
--end_ignore

create table sirv_test13_result2  as select * from sirv_test13_fun1() as field1,
                                                            sirv_test13_fun2(20000,0) as field2
distributed by (field1);

select * from sirv_test13_result2;




--start_ignore

drop language if exists plpythonu cascade;

drop table if exists sirv_test14_result1;

--end_ignore

CREATE LANGUAGE plpythonu;

CREATE or replace FUNCTION sirv_test14_fun1 ()
  RETURNS text
AS $$
  plpy.execute('CREATE TABLE test_data1 (x INT, y INT) distributed by (x)')
  plpy.execute('CREATE TABLE test_data2 (x INT, y VARCHAR) distributed by (x)')

  for i in range(10):
    plpy.execute('INSERT INTO test_data1 VALUES (' + str(i) + ',' + str(i) + ')')

  for i in range(10):
    plpy.execute('INSERT INTO test_data2 VALUES (' + str(i) + ',\'' + str(i) + '\')')

  result1 = plpy.execute ('SELECT CASE WHEN count(*)>0 then \'PASS\' ELSE \'FAIL\' END FROM test_data1')
  result2 = plpy.execute ('SELECT CASE WHEN count(*)>0 then \'PASS\' ELSE \'FAIL\' END FROM test_data2')

  if ((result1 is not None) & (result2 is not None)):
    result3 = 'PASS'
  else:
    result3 = 'FAIL'

  plpy.execute('DROP TABLE test_data1')
  plpy.execute('DROP TABLE test_data2')
  return result3
  
$$ LANGUAGE plpythonu MODIFIES SQL DATA;



CREATE OR REPLACE FUNCTION sirv_test14_fun2(gnp_diff float8, opt integer)  RETURNS FLOAT[] AS $$
  gnp_curr_val = gnp_diff;
  plpy.execute('CREATE TEMP TABLE l_gnp ( gnp_values FLOAT[]) distributed by (gnp_values)')
  plpy.execute('CREATE TEMP TABLE h_gnp ( gnp_values FLOAT[]) distributed by (gnp_values)')

  plpy.execute('INSERT INTO l_gnp SELECT ARRAY(SELECT gnp FROM country where gnp < ' + str(gnp_diff) + ' order by gnp desc)')
  
  gnp_curr_val = gnp_diff

  for i in range(5):
    plpy.execute('TRUNCATE table h_gnp')
    plpy.execute('INSERT INTO h_gnp SELECT ARRAY(SELECT gnp FROM country where gnp > ' +  str(gnp_curr_val) + ' order by gnp desc LIMIT 50)')
    gnp_curr_val = gnp_curr_val + 500

  if(opt > 0):
    res = plpy.execute('SELECT gnp_values from h_gnp')
  else:
    res = plpy.execute('SELECT gnp_values from l_gnp')
  
  plpy.execute('DROP TABLE l_gnp')
  plpy.execute('DROP TABLE h_gnp')
 
  return res[0]["gnp_values"]

$$ LANGUAGE plpythonu MODIFIES SQL DATA;


--select list
create table sirv_test14_result1 (field1 text, field2 float[]) distributed by (field1);

--start_ignore
explain insert into sirv_test14_result1 select 'sirv_test1:' || sirv_test14_fun1(),
                                          ARRAY[1.0::float,2.0::float] || sirv_test14_fun2(20000,0);
--end_ignore
insert into sirv_test14_result1 select 'sirv_test1:' || sirv_test14_fun1(),
                                          ARRAY[1.0::float,2.0::float] || sirv_test14_fun2(20000,0);

select * from sirv_test14_result1;


--from clause
--start_ignore
explain insert into sirv_test14_result1 select * from sirv_test14_fun1(),sirv_test14_fun2(20000,0);
--end_ignore
insert into sirv_test14_result1 select * from sirv_test14_fun1(),sirv_test14_fun2(20000,0);

select * from sirv_test14_result1;




-- start_matchsubs
-- m/psql:.*ERROR:.*/
-- s/psql:.*ERROR:.*/ERROR_MESSAGE/
-- end_matchsubs

--start_ignore

drop table if exists countries_results;

--end_ignore

CREATE OR REPLACE FUNCTION sirv_test17_fun1 (min_languages integer, min_area float8, min_gnp float8, OUT lang_delta int, OUT area_delta float8, OUT gnp_delta float8)
AS $$

BEGIN

  EXECUTE 'CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code)';

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into lang_delta,area_delta,gnp_delta;
  EXECUTE 'DROP TABLE countries_results';

  RETURN;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;

--ctas with a function returning anonymous composite type

--should fail , not supported currently.




--function returning record
CREATE  TABLE countries_results (country_code text, country_name text, lang_count int, area float, gnp float) distributed by (country_code);

CREATE OR REPLACE FUNCTION sirv_test17_fun2 (min_languages integer, min_area float8, min_gnp float8) RETURNS record
AS $$
DECLARE

res record;

BEGIN

  EXECUTE 'INSERT INTO countries_results 
  	   ( with lang_total as
            ( select count(*) as lang_count,country.code,countrylanguage.countrycode
              from country join countrylanguage on (country.code=countrylanguage.countrycode)
              group by country.code,countrylanguage.countrycode order by country.code
            )
            select country.code,country.name, lang_count, country.surfacearea, country.gnp
            from country left outer join lang_total
            on (lang_total.code = country.code)
            where lang_count > ' || min_languages || ' and country.surfacearea > ' || min_area || ' and country.gnp > ' || min_gnp
            || ' order by lang_total.lang_count desc 
           )';
  

  EXECUTE 'SELECT max(lang_count - ' || min_languages || '), max(area - ' || min_area || '), max(gnp - ' || min_gnp || ') FROM countries_results' into res;


  RETURN res;

END;
$$
    LANGUAGE plpgsql volatile MODIFIES SQL DATA;

--ctas with a function returning record

--should fail , not supported currently.



DROP TABLE countries_results;
