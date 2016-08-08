CREATE TABLE mpp7863 (id int, dat char(8))
DISTRIBUTED BY (id)
PARTITION BY RANGE (dat)
( PARTITION Oct09 START (200910) INCLUSIVE END (200911) EXCLUSIVE ,
PARTITION Nov09 START (200911) INCLUSIVE END (200912) EXCLUSIVE ,
PARTITION Dec09 START (200912) INCLUSIVE END (201001) EXCLUSIVE ,
DEFAULT PARTITION extra);

insert into mpp7863 values(generate_series(1,100),'200910');
insert into mpp7863 values(generate_series(101,200),'200911');
insert into mpp7863 values(generate_series(201,300),'200912');
insert into mpp7863 values(generate_series(301,300300),'');
insert into mpp7863 (id) values(generate_series(300301,600300));
insert into mpp7863 values(generate_series(600301,600400),'201001');

select count(*) from mpp7863_1_prt_extra;
select count(*) from mpp7863_1_prt_extra where dat is null;
select count(*) from mpp7863_1_prt_extra where dat ='';
select count(*) from mpp7863;

alter table mpp7863 split default partition start (201001) inclusive end (201002) exclusive into (partition jan10,default partition);
select count(*) from mpp7863_1_prt_extra where dat is null;
select count(*) from mpp7863_1_prt_extra where dat ='';
select count(*) from mpp7863_1_prt_extra;

select dat,count(*) from mpp7863 group by 1 order by 2,1;

drop table mpp7863;
