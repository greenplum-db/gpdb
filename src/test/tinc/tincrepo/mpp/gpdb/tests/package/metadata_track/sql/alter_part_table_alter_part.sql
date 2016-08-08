CREATE TABLE mdt_part_tbl (
id int,
rank int,
year int,
gender char(1),
count int )
DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
SUBPARTITION BY RANGE (year)
SUBPARTITION TEMPLATE (
SUBPARTITION year1 START (2001),
SUBPARTITION year2 START (2002),
SUBPARTITION year6 START (2006) END (2007) )
(PARTITION girls VALUES ('F'),
PARTITION boys VALUES ('M') );


alter table mdt_part_tbl alter partition girls add default partition gfuture;
alter table mdt_part_tbl alter partition boys add default partition bfuture;

drop table mdt_part_tbl;
