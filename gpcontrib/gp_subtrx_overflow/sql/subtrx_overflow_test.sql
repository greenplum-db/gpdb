drop extension if exists gp_subtrx_overflow;
create extension gp_subtrx_overflow;

drop table if exists t_1352_1;
CREATE TABLE t_1352_1(c1 int) distributed by (c1);
CREATE OR REPLACE FUNCTION insert_data()
returns void AS $$
DECLARE
    i int;
BEGIN
	FOR i in 0..1000
	LOOP
		BEGIN
			INSERT INTO t_1352_1 VALUES(i);
		EXCEPTION
		WHEN UNIQUE_VIOLATION THEN
			NULL;
		END;
	END LOOP;
END;
$$
LANGUAGE plpgsql;

drop table if exists t_1352_2;
create table t_1352_2(c int PRIMARY KEY);
CREATE OR REPLACE FUNCTION transaction_test1()
returns void AS $$
DECLARE i int;
begin
	for i in 0..1000
	loop
		begin
			insert into t_1352_2 values(i);
		exception
			WHEN UNIQUE_VIOLATION THEN
				NULL;
		end;
	end loop;
end;
$$
LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION transaction_test2()
returns void AS $$
DECLARE
    i int;
begin
	for i in 0..1000
	loop
		begin
			create temp table tmptab(c int) distributed by (c);
			drop table tmptab;
		exception
			WHEN others THEN
				NULL;
		end;
	end loop;
end;
$$
LANGUAGE plpgsql;

begin;
select insert_data();
select count(*) from (select * from gp_subtrx_backend)
        as a where length(pids) > 2;
commit;

begin;
select transaction_test1();
select count(*) from (select * from gp_subtrx_backend)
        as a where length(pids) > 2;
commit;

begin;
select transaction_test2();
select count(*) from (select * from gp_subtrx_backend)
        as a where length(pids) > 2;
commit; 