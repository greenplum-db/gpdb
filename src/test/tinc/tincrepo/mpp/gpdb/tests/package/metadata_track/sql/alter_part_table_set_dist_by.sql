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

alter table mdt_test_part1_1_prt_1_2_prt_1 set distributed randomly;

ALTER TABLE mdt_test_part1 ALTER PARTITION FOR('M'::bpchar) alter PARTITION FOR(RANK(1)) set distributed by (id, gender, year);

drop table mdt_test_part1;
