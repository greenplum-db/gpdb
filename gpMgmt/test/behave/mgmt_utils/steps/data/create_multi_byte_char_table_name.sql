drop table if exists 测试;
create table 测试(id int);
insert into 测试 select generate_series(1, 1000);
