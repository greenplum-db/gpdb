create schema alter_rollback_@amname@;
set search_path="$user",alter_rollback_@amname@,public;
SET default_table_access_method=@amname@;
-- 
-- Add column in a transaction and then rollback the transaction
--
begin;
create table myao_rollback(i int , j int);
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
select count(*) from myao_rollback;
commit;
begin transaction;
alter table myao_rollback add column k int default 0;
insert into myao_rollback select i , i + 10, i - 10 from generate_series(1,10) i;
select count(*) from myao_rollback;
rollback;
select count(*) from myao_rollback;
