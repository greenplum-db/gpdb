drop table if exists MPP_11341;
create table MPP_11341 (i int, j int) with (appendonly = true)
partition by range (j)
( partition aa start (0) end(1000) every(500) with(appendonly=true)
);

insert into MPP_11341 values(1,1);
select * from MPP_11341_1_prt_aa_1;
insert into MPP_11341_1_prt_aa_1 values(1,1);
select * from MPP_11341_1_prt_aa_1;
