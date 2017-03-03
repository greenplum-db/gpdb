drop table if exists 测试;
create table 测试(id int);
insert into 测试 select generate_series(1, 1000);

drop schema if exists Ž_schema;
drop table if exists Ž_schema.Ž;
create schema Ž_schema;
create table Ž_schema.Ž(id int);
insert into Ž_schema.Ž select generate_series(1, 1000);

drop schema if exists Ж_schema;
drop table if exists Ж_schema.Ж;
create schema Ж_schema;
create table Ж_schema.Ж(id int);
insert into Ж_schema.Ж select generate_series(1, 1000);

drop schema if exists Áá_schema;
drop table if exists Áá_schema.Áá;
create schema Áá_schema;
create table Áá_schema.Áá(id int);
insert into Áá_schema.Áá select generate_series(1, 1000);
