-- @Description Out of order Update (MPP-21144)
-- 


-- scenario 1
drop table if exists uao_ooo_del_tab1;
create table uao_ooo_del_tab1 (i int, j int ) with (appendonly=true);
drop table if exists uao_ooo_del_tab2;
create table uao_ooo_del_tab2 (i int, j int )  with (appendonly=true);
insert into  uao_ooo_del_tab1 values (1,10);
insert  into uao_ooo_del_tab2 values (1,11),(1,22);
select * from uao_ooo_del_tab1 order by 2;
select * from uao_ooo_del_tab2 order by 2;
delete from uao_ooo_del_tab1 using uao_ooo_del_tab2 where uao_ooo_del_tab1.i = uao_ooo_del_tab2.i;
select count(*) from uao_ooo_del_tab2 ;
select count(*) from uao_ooo_del_tab1 ;
set gp_select_invisible = true;
select count(*) from uao_ooo_del_tab1 ;
set gp_select_invisible = false;
-- scenario 2
drop table if exists uao_ooo_del_tab1;
create table uao_ooo_del_tab1 (i int, j int ) with (appendonly=true);;
drop table if exists uao_ooo_del_tab2;
create table uao_ooo_del_tab2 (i int, j int ) with (appendonly=true); ;
insert into  uao_ooo_del_tab1 values (1,10) ,(1,0) ;
update uao_ooo_del_tab1 set j = j + 2 where j = 0;
update uao_ooo_del_tab1 set j = j + 3 where j = 10;
insert  into uao_ooo_del_tab2 values (1,11),(1,22),(1,33),(1,44),(1,55) ;
update uao_ooo_del_tab2 set j = j + 1 where j = 11;
update uao_ooo_del_tab2 set j = j + 2 where j = 22;
select * from uao_ooo_del_tab1 order by 2;
select * from uao_ooo_del_tab2 order by 2;
delete from uao_ooo_del_tab1 using uao_ooo_del_tab2 where uao_ooo_del_tab1.i = uao_ooo_del_tab2.i;
select count(*) from uao_ooo_del_tab2 ;
select count(*) from uao_ooo_del_tab1 ;
set gp_select_invisible = true;
select count(*) from uao_ooo_del_tab1 ;
set gp_select_invisible = false;
vacuum uao_ooo_del_tab1;
set gp_select_invisible = true;
select count(*) from uao_ooo_del_tab1 ;
set gp_select_invisible = false;
-- scenario 3
drop table if exists uao_ooo_upd_tab1;
create table uao_ooo_upd_tab1 (i int, j int ) with (appendonly=true) ;
drop table if exists uao_ooo_upd_tab2;
create table uao_ooo_upd_tab2 (i int, j int ) with (appendonly=true) ;
insert into  uao_ooo_upd_tab1 values (1,10) ,(1,10) ;
insert  into uao_ooo_upd_tab2 values (1,11),(1,22),(1,33),(1,44),(1,55)  ;
BEGIN;
update uao_ooo_upd_tab1 set j = j + 3 where j = 10;
select * from uao_ooo_upd_tab1 order by 2;
select * from uao_ooo_upd_tab2 order by 2;
delete from uao_ooo_del_tab1 using uao_ooo_del_tab2 where uao_ooo_del_tab1.i = uao_ooo_del_tab2.i;
select count(*) from uao_ooo_upd_tab2 ;
select count(*) from uao_ooo_upd_tab1 ;
select * from uao_ooo_upd_tab1 order by 2;
ROLLBACK;
set gp_select_invisible = true;
select count(*) from uao_ooo_upd_tab1 ;
select * from uao_ooo_upd_tab1 order by 2;
set gp_select_invisible = false;
vacuum uao_ooo_upd_tab1;
set gp_select_invisible = true;
select count(*) from uao_ooo_upd_tab1 ;
set gp_select_invisible = false;
