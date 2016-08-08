-- @Description Out of order Update (MPP-21144)
-- 


-- scenario 1
drop table if exists heap_ooo_upd_tab1;
create table heap_ooo_upd_tab1 (i int, j int ) ;
drop table if exists heap_ooo_upd_tab2;
create table heap_ooo_upd_tab2 (i int, j int ) ;
insert into  heap_ooo_upd_tab1 values (1,10);
insert  into heap_ooo_upd_tab2 values (1,11),(1,22);
select * from heap_ooo_upd_tab1 order by 2;
select * from heap_ooo_upd_tab2 order by 2;
update heap_ooo_upd_tab1 set j = heap_ooo_upd_tab2.j from heap_ooo_upd_tab2 where heap_ooo_upd_tab1.i = heap_ooo_upd_tab2.i ;
select count(*) from heap_ooo_upd_tab2 ;
select count(*) from heap_ooo_upd_tab1 ;
set gp_select_invisible = true;
select count(*) from heap_ooo_upd_tab1 ;
set gp_select_invisible = false;
-- scenario 2
drop table if exists heap_ooo_upd_tab1;
create table heap_ooo_upd_tab1 (i int, j int );
drop table if exists heap_ooo_upd_tab2;
create table heap_ooo_upd_tab2 (i int, j int ) ;
insert into  heap_ooo_upd_tab1 values (1,10) ,(1,10) ;
insert  into heap_ooo_upd_tab2 values (1,11),(1,22) ;
select * from heap_ooo_upd_tab1 order by 2;
select * from heap_ooo_upd_tab2 order by 2;
update heap_ooo_upd_tab1 set j = heap_ooo_upd_tab2.j from heap_ooo_upd_tab2 where heap_ooo_upd_tab1.i = heap_ooo_upd_tab2.i ;
select count(*) from heap_ooo_upd_tab2 ;
select count(*) from heap_ooo_upd_tab1 ;
set gp_select_invisible = true;
select count(*) from heap_ooo_upd_tab1 ;
set gp_select_invisible = false;
vacuum heap_ooo_upd_tab1;
set gp_select_invisible = true;
select count(*) from heap_ooo_upd_tab1 ;
set gp_select_invisible = false;

