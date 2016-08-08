-- @Description Out of order Update (MPP-21144)
-- 


-- scenario 1
drop table if exists uaocs_ooo_upd_tab1;
create table uaocs_ooo_upd_tab1 (i int, j int ) with (appendonly=true , orientation=column);
drop table if exists uaocs_ooo_upd_tab2;
create table uaocs_ooo_upd_tab2 (i int, j int )  with (appendonly=true , orientation=column);
insert into  uaocs_ooo_upd_tab1 values (1,10);
insert  into uaocs_ooo_upd_tab2 values (1,11),(1,22);
select * from uaocs_ooo_upd_tab1 order by 2;
select * from uaocs_ooo_upd_tab2 order by 2;
update uaocs_ooo_upd_tab1 set j = uaocs_ooo_upd_tab2.j from uaocs_ooo_upd_tab2 where uaocs_ooo_upd_tab1.i = uaocs_ooo_upd_tab2.i ;
select count(*) from uaocs_ooo_upd_tab2 ;
select count(*) from uaocs_ooo_upd_tab1 ;
set gp_select_invisible = true;
select count(*) from uaocs_ooo_upd_tab1 ;
set gp_select_invisible = false;
-- scenario 2
drop table if exists uaocs_ooo_upd_tab1;
create table uaocs_ooo_upd_tab1 (i int, j int ) with (appendonly=true , orientation=column) ;
drop table if exists uaocs_ooo_upd_tab2;
create table uaocs_ooo_upd_tab2 (i int, j int ) with (appendonly=true , orientation=column) ;
insert into  uaocs_ooo_upd_tab1 values (1,10) ,(1,10) ;
update uaocs_ooo_upd_tab1 set j = j + 3 where j = 10;
insert  into uaocs_ooo_upd_tab2 values (1,11),(1,22),(1,33),(1,44),(1,55)  ;
select * from uaocs_ooo_upd_tab1 order by 2;
select * from uaocs_ooo_upd_tab2 order by 2;
update uaocs_ooo_upd_tab1 set j = uaocs_ooo_upd_tab2.j from uaocs_ooo_upd_tab2 where uaocs_ooo_upd_tab1.i = uaocs_ooo_upd_tab2.i ;
select count(*) from uaocs_ooo_upd_tab2 ;
select count(*) from uaocs_ooo_upd_tab1 ;
set gp_select_invisible = true;
select count(*) from uaocs_ooo_upd_tab1 ;
set gp_select_invisible = false;
vacuum uaocs_ooo_upd_tab1;
set gp_select_invisible = true;
select count(*) from uaocs_ooo_upd_tab1 ;
set gp_select_invisible = false;

-- scenario 3
drop table if exists uaocs_ooo_upd_tab1;
create table uaocs_ooo_upd_tab1 (i int, j int ) with (appendonly=true , orientation=column) ;
drop table if exists uaocs_ooo_upd_tab2;
create table uaocs_ooo_upd_tab2 (i int, j int ) with (appendonly=true , orientation=column) ;
insert into  uaocs_ooo_upd_tab1 values (1,10) ,(1,10) ;
insert  into uaocs_ooo_upd_tab2 values (1,11),(1,22),(1,33),(1,44),(1,55)  ;
BEGIN;
update uaocs_ooo_upd_tab1 set j = j + 3 where j = 10;
select * from uaocs_ooo_upd_tab1 order by 2;
select * from uaocs_ooo_upd_tab2 order by 2;
update uaocs_ooo_upd_tab1 set j = uaocs_ooo_upd_tab2.j from uaocs_ooo_upd_tab2 where uaocs_ooo_upd_tab1.i = uaocs_ooo_upd_tab2.i ;
select count(*) from uaocs_ooo_upd_tab2 ;
select count(*) from uaocs_ooo_upd_tab1 ;
select * from uaocs_ooo_upd_tab1 order by 2;
ROLLBACK;
set gp_select_invisible = true;
select count(*) from uaocs_ooo_upd_tab1 ;
select * from uaocs_ooo_upd_tab1 order by 2;
set gp_select_invisible = false;
vacuum uaocs_ooo_upd_tab1;
set gp_select_invisible = true;
select count(*) from uaocs_ooo_upd_tab1 ;
set gp_select_invisible = false;
