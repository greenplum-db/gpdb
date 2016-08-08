CREATE TABLE mdt_test_part1 (id int, rank int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);

alter table mdt_test_part1 add column BBB int;
alter table mdt_test_part1 DROP column BBB;

drop table mdt_test_part1;
