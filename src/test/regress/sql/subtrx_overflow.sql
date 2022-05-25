-- The case is created using plpgsql. We execute the insert_data() funcion on
-- all segments. It insert data into the different segments.
-- We can see the `begin...exception...end` clause where the exception is
-- not exectued. So, it has 1000 active subtransaction for a transaction.
-- It need back and force from disk and shared memory when we need inspect
-- the snapshot visibility, which may occur stall for the systems.
-- We call the function using the following the SQL statement:
-- begin;
-- select insert_data();
-- end;
-- NOTE: It occur the all segments when subtransactions overflow.
DROP TABLE IF EXISTS t_13524_1;
CREATE TABLE t_13524_1(c1 int) distributed by (c1);
CREATE OR REPLACE FUNCTION insert_data()
returns void AS $$
DECLARE
    i int;
BEGIN
	FOR i in 0..1000
	LOOP
		BEGIN
			INSERT INTO t_13524_1 VALUES(i);
		EXCEPTION
		WHEN UNIQUE_VIOLATION THEN
			NULL;
		END;
	END LOOP;
END;
$$
LANGUAGE plpgsql;

-- This function use the plpgsql also. And it create temp table, which lead to
-- modify the catalog. The coordinator may occur subtransactions overflow if
-- we modify the system catalog. And then, we also insert data into all segments.
-- It leads to overflow of coordinator and segments.
CREATE PROCEDURE transaction_test1()
AS $$
DECLARE
    i int;
begin
	for i in 0..1000
	loop
		begin
			create temp table tmptab(c int) distributed by (c);
			drop table tmptab;
			insert into t_13524_1 values(i);
		exception
			WHEN others THEN
				NULL;
		end;
	end loop;
end;
$$
LANGUAGE plpgsql;


-- This is corner case when the number of active subtransactions has not exceed
-- 64. The `exception` clause is going to be executed When we call function.
-- It leads to the subtransactions abort. According to the postgres rules, it
-- has not subtransactions overflow problem.
DROP TABLE IF EXISTS t_13524_2;
create table t_13524_2(c int PRIMARY KEY);
CREATE PROCEDURE transaction_test2()
AS $$
DECLARE i int;
begin
	for i in 0..1000
	loop
		begin
			insert into t_13524_2 values(1);
		exception
			WHEN UNIQUE_VIOLATION THEN
				NULL;
		end;
	end loop;
end;
$$
LANGUAGE plpgsql;


begin;
select insert_data();
select count(*) from (select (i).segid, (i).pids
    from (select gp_find_subtx_overflowed())a(i) where length((i).pids) > 2)
        as test;
commit;

begin;
call transaction_test1();
select count(*) from (select (i).segid, (i).pids
    from (select gp_find_subtx_overflowed())a(i) where length((i).pids) > 2)
        as test;
commit;

begin;
call transaction_test2();
select count(*) from (select (i).segid, (i).pids
    from (select gp_find_subtx_overflowed())a(i) where length((i).pids) > 2)
        as test;
commit;