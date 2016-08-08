-- 
-- @created 2014-04-14 12:00:00
-- @modified 2014-04-14 12:00:00
-- @tags storage
-- @description AOCO table: Alter add column in a transaction and then rollback the transaction

Drop table if exists myao_rollback;
create table myao_rollback(i int , j int) with (appendonly=true, orientation=column);
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
insert into myao_rollback select i, i+10 from generate_series(1,2000) i ;
select count(*) from myao_rollback;
\d myao_rollback
begin transaction;
alter table myao_rollback add column k int default 0;
insert into myao_rollback select i , i + 10, i - 10 from generate_series(1,10) i;
select count(*) from myao_rollback;
\d myao_rollback
rollback;
select count(*) from myao_rollback;
\d myao_rollback
