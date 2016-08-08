Drop tablespace subt_tbsp_co_new;
create tablespace subt_tbsp_co_new filespace subt_filespace_a ;
BEGIN;

Savepoint sp1;
Drop table if exists subt_alter_table_ao1;
Create table subt_alter_table_ao1 (col0 int) with(appendonly=true, orientation = column) distributed randomly;
Insert into subt_alter_table_ao1 values(generate_series(1,5));
select count(*) from subt_alter_table_ao1;
select tablespace from pg_tables where tablename ='subt_alter_table_ao1';


Savepoint sp2;
Alter table subt_alter_table_ao1 add column subt_aol1 int default 10 ;
Alter table subt_alter_table_ao1 add column subt_aol2 int default 10 ;
select tablespace from pg_tables where tablename ='subt_alter_table_ao1';

Alter table subt_alter_table_ao1 alter column subt_aol1 type numeric ;
Alter table subt_alter_table_ao1 rename column subt_aol1 to new_aol1 ;
Alter table subt_alter_table_ao1 drop column new_aol1 ;
Alter table subt_alter_table_ao1 set tablespace subt_tbsp_co_new;

select tablespace from pg_tables where tablename ='subt_alter_table_ao1';

Savepoint sp3;

select count(*) from subt_alter_table_ao1;
select tablespace from pg_tables where tablename ='subt_alter_table_ao1';

Rollback to sp1;
select count(*) from subt_alter_table_ao1;
select tablespace from pg_tables where tablename ='subt_alter_table_ao1';

COMMIT;
