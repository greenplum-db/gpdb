-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

drop table if exists  sto_uao_taba;
create table sto_uao_taba (deptno int) with (appendonly=true);
insert  into sto_uao_taba select deptno from generate_series(1,5) as deptno;

drop table if exists  sto_uao_tabb;
create table sto_uao_tabb (deptno int) with (appendonly=true);
insert into sto_uao_tabb select deptno  from generate_series(2,4) as deptno;

select count(*) AS only_visi_tups_ins  from sto_uao_taba;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from sto_uao_taba;
set gp_select_invisible = false;

 DELETE
    FROM    sto_uao_taba a
    USING   sto_uao_tabb b
    WHERE   a.deptno = b.deptno;
    

select count(*) AS only_visi_tups_del  from sto_uao_taba;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_del  from sto_uao_taba;
set gp_select_invisible = false;

vacuum sto_uao_taba;

select count(*) AS only_visi_tups_vacuum  from sto_uao_taba;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sto_uao_taba;
set gp_select_invisible = false;

