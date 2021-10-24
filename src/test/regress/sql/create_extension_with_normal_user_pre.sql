drop table if exists t;
drop user if exists normal_user;
create user normal_user;
--1. use normal user to create extension
--2. create a table
--3. quit psql
--4. reconnect
--5. try to drop the table
--1--3 steps
set role normal_user;
create extension file_fdw;
create table t(name text);
