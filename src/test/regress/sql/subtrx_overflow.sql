drop table if exists t_135244_1;
CREATE TABLE t_135244_1(c1 int) distributed by (c1);
CREATE OR REPLACE FUNCTION insert_data()
returns void AS $$
DECLARE
    i int;
BEGIN
	FOR i in 0..1000
	LOOP
		BEGIN
			INSERT INTO t_135244_1 VALUES(i);
		EXCEPTION
		WHEN UNIQUE_VIOLATION THEN
			NULL;
		END;
	END LOOP;
END;
$$
LANGUAGE plpgsql;


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
			insert into t_135244_1 values(i);
		exception
			WHEN others THEN
				NULL;
		end;
	end loop;
end;
$$
LANGUAGE plpgsql;


drop table if exists t_135244_2;
create table t_135244_2(c int PRIMARY KEY);
CREATE PROCEDURE transaction_test2()
AS $$
DECLARE i int;
begin
	for i in 0..1000
	loop
		begin
			insert into t_135244_2 values(1);
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
select count(*) from (select * from gp_subtrx_overflow_pids())
        as a where length(pids) > 0;
commit;

begin;
call transaction_test1();
select count(*) from (select * from gp_subtrx_overflow_pids())
        as a where length(pids) > 0;
commit;

begin;
call transaction_test2();
select count(*) from (select * from gp_subtrx_overflow_pids())
        as a where length(pids) > 0;
commit;