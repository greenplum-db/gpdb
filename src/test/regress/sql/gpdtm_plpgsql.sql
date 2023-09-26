--
-- Here's a version of Nitasha's transaction test (which turned up lots and lots of
-- interesting issues). 
--
-- Create some tables with some data
create table tabcd (c1 text) distributed randomly;
insert into tabcd values ('a'), ('b'), ('c'), ('d');

create table t1234 (c1 integer) distributed randomly;
insert into t1234 values (1),(2),(3),(4);

create table tabcd_orig as select * from tabcd distributed randomly;
create table t1234_orig as select * from t1234 distributed randomly;

-- Intermix selects, and truncates with calls to a plpgsql function,
-- functions like this are an interesting challenge, since they get
-- dispatched with a single command-id.
--
-- This function does a series of updates.
--create language plpgsql;
CREATE OR REPLACE FUNCTION transaction_test_cursor_nit() RETURNS void AS '
DECLARE
	ref_abcd refcursor;
	ref_1234_1 refcursor;
	ref_1234_2 refcursor;
	abcd_var varchar;
	t_1234_var_1 int;
	t1234_var_2 int;
	i int;
	j int;
	arr_1234 int [4];
	arr_abcd varchar [4];
BEGIN
	arr_1234[1]:=1;
	arr_1234[2]:=2;
	arr_1234[3]:=3;
	arr_1234[4]:=4;

	open ref_1234_1 FOR SELECT c1 FROM t1234 order by 1;

	BEGIN
		j:=1;
		open ref_abcd FOR SELECT c1 FROM tabcd order by 1;
		fetch ref_abcd into abcd_var;
		while abcd_var is not null loop
		      arr_abcd[j]:=abcd_var;
		      BEGIN
				open ref_1234_2 FOR SELECT c1 FROM t1234 order by 1;
				fetch ref_1234_2 into t1234_var_2;
				i:=1;
				while t1234_var_2 is not null loop
				      update tabcd set c1=c1||t1234_var_2 where c1=arr_abcd[j];
		      		      arr_abcd[j]:=arr_abcd[j]||t1234_var_2;
				      arr_1234[i]:=arr_1234[i]+10;
		  		      i:=i+1;
				      fetch ref_1234_2 into t1234_var_2;
				end loop;
				close ref_1234_2;
			END;
			fetch ref_abcd into abcd_var;
			j:=j+1;
		end loop;
		close ref_abcd;

	        close ref_1234_1;
		open ref_1234_1 FOR SELECT c1 FROM t1234 order by 1;
		fetch ref_1234_1 into t_1234_var_1;

		while t_1234_var_1 is not null loop
--		      raise notice ''in while set index % % arg where %'', t_1234_var_1,  arr_1234[t_1234_var_1], t_1234_var_1;
		      update t1234 set c1 = arr_1234[t_1234_var_1] where c1 = t_1234_var_1;
		      fetch ref_1234_1 into t_1234_var_1;
	      end loop;

      END;
      close ref_1234_1;
END;
' LANGUAGE plpgsql MODIFIES SQL DATA;

-- encourage reader-gangs to win races:
SET gp_enable_slow_writer_testmode=on;

-- Now here's the main piece of the test.
TRUNCATE tabcd;
TRUNCATE t1234;
INSERT INTO tabcd SELECT * from tabcd_orig;
INSERT INTO t1234 SELECT * from t1234_orig;
SELECT transaction_test_cursor_nit();
SELECT * from tabcd order by 1;
SELECT * from t1234 order by 1;

BEGIN;
TRUNCATE tabcd;
TRUNCATE t1234;
INSERT INTO tabcd SELECT * from tabcd_orig;
INSERT INTO t1234 SELECT * from t1234_orig;
SELECT * from t1234 order by 1;
SELECT transaction_test_cursor_nit();
SELECT * from tabcd order by 1;
SELECT * from t1234 order by 1;
COMMIT;

BEGIN;
set transaction isolation level serializable;
TRUNCATE tabcd;
TRUNCATE t1234;
INSERT INTO tabcd SELECT * from tabcd_orig;
INSERT INTO t1234 SELECT * from t1234_orig;
SELECT * from t1234 order by 1;
SELECT transaction_test_cursor_nit();
SELECT * from tabcd order by 1;
SELECT * from t1234 order by 1;
COMMIT;

SELECT * from tabcd order by 1;
SELECT * from t1234 order by 1;

-- refresh the data 
TRUNCATE t1234;
INSERT INTO t1234 SELECT * from t1234_orig;


create table t3456 as select c1 from t1234_orig distributed by (c1);

-- Create some reader gangs.
select a.c1 from t1234 a, t3456 b where a.c1 = b.c1 order by b.c1;

BEGIN;
TRUNCATE t1234;
select a.c1 from t1234 a, t3456 b where a.c1 = b.c1 order by b.c1; -- should return 0 rows.

INSERT INTO t1234 SELECT * from t1234_orig;
TRUNCATE t3456;
select a.c1 from t1234 a, t3456 b where a.c1 = b.c1 order by b.c1; -- should return 0 rows.
ABORT;

select a.c1 from t1234 a, t3456 b where a.c1 = b.c1 order by b.c1;

CREATE OR REPLACE FUNCTION foo_func() RETURNS void AS '
BEGIN
update dtm_plpg_foo set a = a + 3;
insert into dtm_plpg_baz select dtm_plpg_bar.b from dtm_plpg_bar where dtm_plpg_bar.b >= (select max(a) from dtm_plpg_foo);
update dtm_plpg_foo set a = a + 3;
insert into dtm_plpg_baz select dtm_plpg_bar.b from dtm_plpg_bar where dtm_plpg_bar.b >= (select max(a) from dtm_plpg_foo);
END;
' LANGUAGE plpgsql MODIFIES SQL DATA;

create table dtm_plpg_foo (a int) distributed randomly;
create table dtm_plpg_bar (b int) distributed randomly;
create table dtm_plpg_baz (c int) distributed randomly;

insert into dtm_plpg_foo values (1), (2), (3), (4), (5), (6);
insert into dtm_plpg_bar values (5), (6), (7), (8), (9), (10);

insert into dtm_plpg_baz select dtm_plpg_bar.b from dtm_plpg_bar where dtm_plpg_bar.b >= (select max(a) from dtm_plpg_foo);
select * from dtm_plpg_baz order by 1;

truncate dtm_plpg_baz;
begin;
update dtm_plpg_foo set a = a + 3;
insert into dtm_plpg_baz select dtm_plpg_bar.b from dtm_plpg_bar where dtm_plpg_bar.b >= (select max(a) from dtm_plpg_foo);
select * from dtm_plpg_baz order by 1;
abort;

truncate dtm_plpg_baz;
begin;
set transaction isolation level serializable;
update dtm_plpg_foo set a = a + 3;
insert into dtm_plpg_baz select dtm_plpg_bar.b from dtm_plpg_bar where dtm_plpg_bar.b >= (select max(a) from dtm_plpg_foo);
select * from dtm_plpg_baz order by 1;
update dtm_plpg_foo set a = a + 3;
insert into dtm_plpg_baz select dtm_plpg_bar.b from dtm_plpg_bar where dtm_plpg_bar.b >= (select max(a) from dtm_plpg_foo);
select * from dtm_plpg_baz order by 1;
abort;

truncate dtm_plpg_baz;
begin;
select * from dtm_plpg_foo order by 1;
select * from dtm_plpg_bar order by 1;
select foo_func();
select * from dtm_plpg_foo order by 1;
select * from dtm_plpg_baz order by 1;
abort;

truncate dtm_plpg_baz;
begin;
set transaction isolation level serializable;
select * from dtm_plpg_foo order by 1;
select * from dtm_plpg_bar order by 1;
select foo_func();
select * from dtm_plpg_foo order by 1;
select * from dtm_plpg_baz order by 1;
abort;

DROP TABLE dtm_plpg_foo;

-- Need to check what these tests wish to validate, better to use more
-- deterministic way than sleep. GP_ENABLE_SLOW_CURSOR_TESTMODE GUC was used
-- here to slow down reader gangs, removed the same as its not good way to write
-- the tests.

BEGIN;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
CREATE TABLE dtm_plpg_foo (a int, b int);
INSERT INTO dtm_plpg_foo VALUES (0, 1);
DECLARE c1 NO SCROLL CURSOR FOR SELECT b FROM dtm_plpg_foo;
UPDATE dtm_plpg_foo SET b = 2;
FETCH ALL FROM c1;
COMMIT;

DROP TABLE dtm_plpg_foo;

BEGIN;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
CREATE TABLE dtm_plpg_foo (a int, b int);
INSERT INTO dtm_plpg_foo VALUES (0, 1);
UPDATE dtm_plpg_foo SET b = 2;
DECLARE c1 NO SCROLL CURSOR FOR SELECT b FROM dtm_plpg_foo;
UPDATE dtm_plpg_foo SET b = 3;
FETCH ALL FROM c1;
COMMIT;

DROP TABLE dtm_plpg_foo;

BEGIN;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
CREATE TABLE dtm_plpg_foo (a int, b int);
INSERT INTO dtm_plpg_foo VALUES (0, 1);
UPDATE dtm_plpg_foo SET b = 2;
DECLARE c1 NO SCROLL CURSOR FOR SELECT b FROM dtm_plpg_foo;
DELETE FROM dtm_plpg_foo;
INSERT INTO dtm_plpg_foo VALUES (0, 3);
UPDATE dtm_plpg_foo SET b = 4;
INSERT INTO dtm_plpg_foo VALUES (0, 5);
FETCH ALL FROM c1;
COMMIT;

-- Add some additional testing for combocid-issues
DROP TABLE dtm_plpg_foo;
BEGIN;
CREATE TABLE dtm_plpg_foo (C_CUSTKEY INTEGER, C_NAME VARCHAR(25), C_ADDRESS VARCHAR(40))
partition by range (c_custkey) (partition p1 start(0) end(100000) every(20000));

INSERT INTO dtm_plpg_foo SELECT * FROM dtm_plpg_foo LIMIT 10000;
COMMIT;

BEGIN;
DROP TABLE dtm_plpg_foo;
CREATE TABLE dtm_plpg_foo (C_CUSTKEY INTEGER, C_NAME VARCHAR(25), C_ADDRESS VARCHAR(40))
partition by range (c_custkey) (partition p1 start(0) end(100000) every(20000));
SELECT count(*) from dtm_plpg_foo;
INSERT INTO dtm_plpg_foo SELECT * FROM dtm_plpg_foo LIMIT 10000;
COMMIT;

BEGIN;
DROP TABLE dtm_plpg_foo;
CREATE TABLE dtm_plpg_foo (C_CUSTKEY INTEGER, C_NAME VARCHAR(25), C_ADDRESS VARCHAR(40))
partition by range (c_custkey) (partition p1 start(0) end(100000) every(20000));
DECLARE c0 CURSOR FOR SELECT * FROM dtm_plpg_foo;
INSERT INTO dtm_plpg_foo SELECT * FROM dtm_plpg_foo LIMIT 10000;
FETCH c0;
CLOSE c0;
COMMIT;

BEGIN;
DROP TABLE dtm_plpg_foo;
CREATE TABLE dtm_plpg_foo (C_CUSTKEY INTEGER, C_NAME VARCHAR(25), C_ADDRESS VARCHAR(40))
partition by range (c_custkey) (partition p1 start(0) end(100000) every(20000));
DECLARE c0 CURSOR WITH HOLD FOR SELECT * FROM dtm_plpg_foo;
INSERT INTO dtm_plpg_foo SELECT * FROM dtm_plpg_foo LIMIT 10000;
FETCH c0;
CLOSE c0;
COMMIT;
--
-- Sequence server test for an extended query
--

CREATE OR REPLACE function parse_arr (p_array text)
	returns int[] IMMUTABLE
	AS $dbvis$
	declare
		v_return int[][];
		v_text text;
		v_first int; 
	BEGIN
		if p_array is null 
		then 
			return null;
		end if;  

		v_first := 1;
		for v_text in
			select unnest(string_to_array(p_array, ';'))
		loop 
			v_return := v_return || ARRAY[(string_to_array(regexp_replace(v_text, '[)( ]', '', 'g'), ',')::int[])];
		end loop;
   
		RETURN v_return;
	END; 
$dbvis$ LANGUAGE plpgsql;

CREATE TABLE test_parse_arr (a bigserial, b int[]);

INSERT INTO test_parse_arr (b)
	SELECT parse_arr(x) FROM 
				(    
				  SELECT '(1, 2, 3)' AS x 
				  UNION ALL
				  SELECT NULL 
				) AS q;

SELECT b FROM test_parse_arr;

--
-- Test if sequence server information outlives a plpgsql exception and corresponding subtransaction rollback (MPP-25193)
--

CREATE OR REPLACE FUNCTION date_converter(input_date character varying) RETURNS date
    AS $$
declare
 v_date date;
BEGIN
	return input_date::date;
exception
	when others then
		return '1900-01-01';
END;
$$
LANGUAGE plpgsql SECURITY DEFINER;

CREATE TABLE source_table
(
id int,
created text
);

insert into source_table select generate_series(1,10), '2015-01-06'::date;
insert into source_table values (11, '0000-00-00');
insert into source_table select generate_series(12,21), '2015-01-06'::date;

CREATE SEQUENCE test_seq START 1;

CREATE TABLE dest_table
(
id int,
created timestamp with time zone
);

insert into dest_table select nextval('test_seq'::regclass), date_converter(created) from source_table;
select count(1) from dest_table;


RESET gp_enable_slow_writer_testmode;

--
-- The PL/pgSQL EXCEPTION block opens a subtransaction.
-- If it's in reader, it was messing up relcache previously.
--
create table stran_foo(a, b) as values(1, 10), (2, 20);
create or replace function stran_func(a int) returns int as $$
declare
  x int;
begin
  begin
    select 1 + 2 into x;
  exception
    when division_by_zero then
      raise info 'except';
  end;
  return x;
end;
$$ language plpgsql;

create table stran_tt as select stran_func(b) from stran_foo;


--
-- Check quoting when dispatching savepoints. (Not really PL/pgSQL related,
-- but here for lack of a better place.)
--
BEGIN;
CREATE TEMPORARY TABLE savepointtest (t text);
INSERT INTO savepointtest VALUES ('before savepoints');
SAVEPOINT "evil""savepoint1";
INSERT INTO savepointtest VALUES ('after sp 1');
SAVEPOINT "evil""savepoint2";
INSERT INTO savepointtest VALUES ('after sp 2');
ROLLBACK TO SAVEPOINT "evil""savepoint2";
RELEASE SAVEPOINT "evil""savepoint1";
INSERT INTO savepointtest VALUES ('back to top transaction');
COMMIT;
SELECT * FROM savepointtest;
