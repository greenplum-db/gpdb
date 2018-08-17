--
-- Test cases for COPY (select) TO
--
create table copyselect_test1 (id serial, t text);
insert into copyselect_test1 (t) values ('a');
insert into copyselect_test1 (t) values ('b');
insert into copyselect_test1 (t) values ('c');
insert into copyselect_test1 (t) values ('d');
insert into copyselect_test1 (t) values ('e');

create table copyselect_test2 (id serial, t text);
insert into copyselect_test2 (t) values ('A');
insert into copyselect_test2 (t) values ('B');
insert into copyselect_test2 (t) values ('C');
insert into copyselect_test2 (t) values ('D');
insert into copyselect_test2 (t) values ('E');

create view v_copyselect_test1
as select 'v_'||t from copyselect_test1;

--
-- Test COPY table TO
--
copy copyselect_test1 to stdout;
--
-- This should fail
--
copy v_copyselect_test1 to stdout;
--
-- Test COPY (select) TO
--
copy (select t from copyselect_test1 where id=1) to stdout;
--
-- Test COPY (select for update) TO
--
copy (select t from copyselect_test1 where id=3 for update) to stdout;
--
-- This should fail
--
copy (select t into temp test3 from copyselect_test1 where id=3) to stdout;
--
-- This should fail
--
copy (select * from copyselect_test1) from stdin;
--
-- This should fail
--
copy (select * from copyselect_test1) (t,id) to stdout;
--
-- Test JOIN
--
copy (select * from copyselect_test1 join copyselect_test2 using (id)) to stdout;
--
-- Test UNION SELECT
--
copy (select t from copyselect_test1 where id = 1 UNION select * from v_copyselect_test1 ORDER BY 1) to stdout;
--
-- Test subselect
--
copy (select * from (select t from copyselect_test1 where id = 1 UNION select * from v_copyselect_test1) t1 ORDER BY 1) to stdout;
--
-- Test headers, CSV and quotes
--
copy (select t from copyselect_test1 where id = 1) to stdout csv header force quote t;
--
-- Test psql builtins, plain table
--
\copy copyselect_test1 to stdout
--
-- This should fail
--
\copy v_copyselect_test1 to stdout
--
-- Test \copy (select ...)
--
\copy (select "id",'id','id""'||t,(id + 1)*id,t,"copyselect_test1"."t" from copyselect_test1 where id=3) to stdout
--
-- Drop everything
--
drop table copyselect_test2;
drop view v_copyselect_test1;
drop table copyselect_test1;

-- psql handling of COPY in multi-command strings
copy (select 1) to stdout\; select 1/0;	-- row, then error
select 1/0\; copy (select 1) to stdout; -- error only
copy (select 1) to stdout\; copy (select 2) to stdout\; select 0\; select 3; -- 1 2 3

create table test3 (c int);
select 0\; copy test3 from stdin\; copy test3 from stdin\; select 1; -- 1
1
\.
2
\.
select * from test3;
drop table test3;
