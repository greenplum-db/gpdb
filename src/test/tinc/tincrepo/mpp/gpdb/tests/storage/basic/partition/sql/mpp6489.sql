CREATE TABLE mpp6489 (id int, rank int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'),
start (date '2002-01-01'),
start (date '2003-01-01'),
start (date '2004-01-01'),
start (date '2005-01-01')
)
(
values ('M'),
values ('F')
);

alter table mpp6489_1_prt_1_2_prt_5 set distributed randomly;

ALTER TABLE "mpp6489" ALTER PARTITION FOR('M'::bpchar) alter PARTITION
FOR(RANK(5)) set distributed by (id, gender, year);

drop table mpp6489;
