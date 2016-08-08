-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description MPP-5458

drop table if exists mpp5458_test;
create table mpp5458_test (i int, b bigserial);
alter table mpp5458_test drop column b;
insert into mpp5458_test values (1);
alter table mpp5458_test add column b bigserial;

drop table if exists mpp5458_t;
create table mpp5458_t as select j as a, 'abc' as i from
generate_series(1, 10) j;

alter table mpp5458_t alter i type int; -- error out

alter table mpp5458_t alter i type text;

--cursor
begin;
declare c1 cursor for select * from mpp5458_t;
-- start_ignore
fetch 1 from c1;

select * from mpp5458_test limit 10;
-- end_ignore

drop table if exists mpp5458_test1;
create table mpp5458_test1 (i int, b bigserial);
alter table mpp5458_test1 drop column b;
insert into mpp5458_test1 values (1);
alter table mpp5458_test1 add column b bigserial;

-- start_ignore

fetch 5 from c1;
fetch 5 from c1;
-- end_ignore

close c1;

end;

drop sequence if exists mpp5458_s1;
create sequence mpp5458_s1;
-- start_ignore
select *, nextval('mpp5458_s1') from mpp5458_t;
-- end_ignore

-- prepare
prepare mpp5458_stmt as select *, nextval('mpp5458_s1') from mpp5458_t;
-- start_ignore
execute mpp5458_stmt;
-- end_ignore

deallocate mpp5458_stmt;

--cursor
begin;
declare mpp5458_c1 cursor for select *, nextval('mpp5458_s1') from mpp5458_t;

-- start_ignore

fetch 1 from mpp5458_c1;

select * from mpp5458_test limit 10;

fetch 10 from mpp5458_c1;

fetch 1 from mpp5458_c1;
-- end_ignore

close mpp5458_c1;
end;
