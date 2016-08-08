create table mpp5308 (i int) partition by range(i) (start(1) end(10) every(1));
\d mpp5308_1_prt_5
alter table mpp5308_1_prt_5 drop constraint mpp5308_1_prt_5_check;
drop table mpp5308;
