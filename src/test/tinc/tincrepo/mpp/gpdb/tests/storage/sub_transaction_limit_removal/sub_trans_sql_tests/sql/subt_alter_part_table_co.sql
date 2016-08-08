BEGIN;
Savepoint sp1;
Drop table if exists subt_alter_part_tab_ao1;
Create table subt_alter_part_tab_ao1 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true, orientation = column) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );

Savepoint sp2;
Alter table subt_alter_part_tab_ao1 add partition p4 start(11) end(15);
Alter table subt_alter_part_tab_ao1 add default partition def_part;
Insert into subt_alter_part_tab_ao1 values(generate_series(1,15),'create table with subtransactions','s','subtransaction table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');

Savepoint sp3;

Select count(*) from subt_alter_part_tab_ao1;
Alter table subt_alter_part_tab_ao1 drop partition p3 ;
Select count(*) from subt_alter_part_tab_ao1;

Alter table subt_alter_part_tab_ao1 drop default partition;
Drop table if exists exg_pt_ao1;
Select count(*) from subt_alter_part_tab_ao1;

Create table exg_pt_ao1(i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone);
Insert into exg_pt_ao1 values (7,'to be exchanged','s','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
Alter table subt_alter_part_tab_ao1 exchange partition p2 with table exg_pt_ao1;

Alter table subt_alter_part_tab_ao1 split partition p4 at (13) into (partition splita,partition splitb);

Savepoint sp4;
Alter table subt_alter_part_tab_ao1 drop partition splita ;
Select count(*) from subt_alter_part_tab_ao1;

rollback to sp3;
Select count(*) from subt_alter_part_tab_ao1;
Alter table subt_alter_part_tab_ao1 drop partition splita ;

COMMIT;
