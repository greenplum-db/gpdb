--
--Drop and Create tables
--
Drop table if exists nested_subtx_t1;
Create table nested_subtx_t1(a1 int,a2 text,a3 char,a4 varchar, a5 date, a6 numeric, a7 timestamp without time zone, a8 time with time zone) distributed randomly;

Drop table if exists nested_subtx_t2;
Create table nested_subtx_t2(b1 int,b2 text,b3 char,b4 varchar, b5 date, b6 numeric, b7 timestamp without time zone, b8 time with time zone) distributed randomly;
Insert into nested_subtx_t2 values(100,'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');

BEGIN;

 Savepoint sp1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp1n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp1n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp1n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp1n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp1n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp1n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp1n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp1;

 Savepoint sp2;
   Savepoint sp2n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp2n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp2n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp2n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp2n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp2n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp2n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp2;

 Savepoint sp3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp3n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp3n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp3n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp3n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp3n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp3n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp3n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp3;

 Savepoint sp4;
   Savepoint sp4n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp4n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp4n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp4n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp4n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp4n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp4n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp4;

 Savepoint sp5;
     Insert into nested_subtx_t1 values(5, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp5n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp5n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp5n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp5n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp5n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp5n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp5n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp5;

 Savepoint sp6;
   Savepoint sp6n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp6n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp6n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp6n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp6n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp6n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp6n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp6;

 Savepoint sp7;
     Insert into nested_subtx_t1 values(7, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp7n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp7n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp7n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp7n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp7n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp7n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp7n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp7;

 Savepoint sp8;
   Savepoint sp8n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp8n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp8n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp8n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp8n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp8n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp8n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp8;

 Savepoint sp9;
     Insert into nested_subtx_t1 values(9, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp9n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp9n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp9n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp9n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp9n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp9n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp9n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp9;

 Savepoint sp10;
   Savepoint sp10n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp10n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp10n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp10n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp10n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp10n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp10n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp10;

 Savepoint sp11;
     Insert into nested_subtx_t1 values(11, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp11n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp11n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp11n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp11n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp11n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp11n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp11n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp11;

 Savepoint sp12;
   Savepoint sp12n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp12n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp12n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp12n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp12n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp12n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp12n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp12;

 Savepoint sp13;
     Insert into nested_subtx_t1 values(13, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp13n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp13n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp13n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp13n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp13n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp13n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp13n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp13;

 Savepoint sp14;
   Savepoint sp14n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp14n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp14n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp14n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp14n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp14n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp14n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp14;

 Savepoint sp15;
     Insert into nested_subtx_t1 values(15, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp15n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp15n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp15n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp15n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp15n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp15n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp15n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp15;

 Savepoint sp16;
   Savepoint sp16n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp16n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp16n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp16n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp16n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp16n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp16n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp16;

 Savepoint sp17;
     Insert into nested_subtx_t1 values(17, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp17n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp17n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp17n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp17n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp17n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp17n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp17n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp17;

 Savepoint sp18;
   Savepoint sp18n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp18n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp18n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp18n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp18n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp18n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp18n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp18;

 Savepoint sp19;
     Insert into nested_subtx_t1 values(19, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp19n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp19n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp19n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp19n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp19n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp19n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp19n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp19;

 Savepoint sp20;
   Savepoint sp20n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp20n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp20n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp20n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp20n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp20n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp20n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp20;

 Savepoint sp21;
     Insert into nested_subtx_t1 values(21, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp21n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp21n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp21n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp21n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp21n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp21n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp21n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp21;

 Savepoint sp22;
   Savepoint sp22n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp22n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp22n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp22n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp22n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp22n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp22n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp22;

 Savepoint sp23;
     Insert into nested_subtx_t1 values(23, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp23n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp23n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp23n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp23n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp23n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp23n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp23n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp23;

 Savepoint sp24;
   Savepoint sp24n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp24n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp24n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp24n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp24n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp24n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp24n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp24;

 Savepoint sp25;
     Insert into nested_subtx_t1 values(25, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp25n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp25n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp25n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp25n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp25n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp25n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp25n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp25;

 Savepoint sp26;
   Savepoint sp26n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp26n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp26n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp26n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp26n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp26n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp26n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp26;

 Savepoint sp27;
     Insert into nested_subtx_t1 values(27, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp27n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp27n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp27n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp27n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp27n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp27n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp27n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp27;

 Savepoint sp28;
   Savepoint sp28n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp28n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp28n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp28n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp28n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp28n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp28n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp28;

 Savepoint sp29;
     Insert into nested_subtx_t1 values(29, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp29n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp29n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp29n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp29n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp29n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp29n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp29n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp29;

 Savepoint sp30;
   Savepoint sp30n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp30n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp30n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp30n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp30n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp30n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp30n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp30;

 Savepoint sp31;
     Insert into nested_subtx_t1 values(31, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp31n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp31n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp31n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp31n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp31n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp31n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp31n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp31;

 Savepoint sp32;
   Savepoint sp32n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp32n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp32n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp32n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp32n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp32n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp32n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp32;

 Savepoint sp33;
     Insert into nested_subtx_t1 values(33, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp33n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp33n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp33n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp33n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp33n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp33n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp33n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp33;

 Savepoint sp34;
   Savepoint sp34n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp34n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp34n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp34n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp34n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp34n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp34n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp34;

 Savepoint sp35;
     Insert into nested_subtx_t1 values(35, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp35n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp35n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp35n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp35n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp35n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp35n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp35n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp35;

 Savepoint sp36;
   Savepoint sp36n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp36n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp36n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp36n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp36n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp36n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp36n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp36;

 Savepoint sp37;
     Insert into nested_subtx_t1 values(37, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp37n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp37n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp37n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp37n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp37n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp37n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp37n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp37;

 Savepoint sp38;
   Savepoint sp38n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp38n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp38n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp38n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp38n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp38n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp38n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp38;

 Savepoint sp39;
     Insert into nested_subtx_t1 values(39, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp39n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp39n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp39n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp39n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp39n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp39n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp39n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp39;

 Savepoint sp40;
   Savepoint sp40n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp40n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp40n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp40n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp40n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp40n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp40n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp40;

 Savepoint sp41;
     Insert into nested_subtx_t1 values(41, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp41n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp41n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp41n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp41n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp41n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp41n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp41n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp41;

 Savepoint sp42;
   Savepoint sp42n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp42n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp42n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp42n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp42n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp42n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp42n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp42;

 Savepoint sp43;
     Insert into nested_subtx_t1 values(43, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp43n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp43n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp43n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp43n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp43n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp43n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp43n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp43;

 Savepoint sp44;
   Savepoint sp44n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp44n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp44n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp44n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp44n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp44n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp44n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp44;

 Savepoint sp45;
     Insert into nested_subtx_t1 values(45, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp45n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp45n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp45n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp45n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp45n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp45n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp45n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp45;

 Savepoint sp46;
   Savepoint sp46n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp46n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp46n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp46n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp46n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp46n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp46n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp46;

 Savepoint sp47;
     Insert into nested_subtx_t1 values(47, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp47n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp47n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp47n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp47n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp47n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp47n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp47n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp47;

 Savepoint sp48;
   Savepoint sp48n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp48n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp48n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp48n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp48n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp48n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp48n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp48;

 Savepoint sp49;
     Insert into nested_subtx_t1 values(49, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp49n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp49n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp49n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp49n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp49n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp49n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp49n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp49;

 Savepoint sp50;
   Savepoint sp50n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp50n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp50n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp50n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp50n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp50n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp50n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp50;

 Savepoint sp51;
     Insert into nested_subtx_t1 values(51, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp51n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp51n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp51n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp51n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp51n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp51n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp51n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp51;

 Savepoint sp52;
   Savepoint sp52n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp52n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp52n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp52n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp52n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp52n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp52n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp52;

 Savepoint sp53;
     Insert into nested_subtx_t1 values(53, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp53n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp53n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp53n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp53n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp53n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp53n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp53n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp53;

 Savepoint sp54;
   Savepoint sp54n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp54n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp54n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp54n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp54n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp54n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp54n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp54;

 Savepoint sp55;
     Insert into nested_subtx_t1 values(55, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp55n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp55n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp55n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp55n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp55n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp55n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp55n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp55;

 Savepoint sp56;
   Savepoint sp56n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp56n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp56n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp56n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp56n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp56n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp56n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp56;

 Savepoint sp57;
     Insert into nested_subtx_t1 values(57, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp57n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp57n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp57n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp57n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp57n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp57n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp57n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp57;

 Savepoint sp58;
   Savepoint sp58n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp58n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp58n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp58n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp58n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp58n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp58n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp58;

 Savepoint sp59;
     Insert into nested_subtx_t1 values(59, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp59n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp59n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp59n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp59n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp59n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp59n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp59n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp59;

 Savepoint sp60;
   Savepoint sp60n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp60n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp60n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp60n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp60n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp60n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp60n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp60;

 Savepoint sp61;
     Insert into nested_subtx_t1 values(61, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp61n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp61n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp61n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp61n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp61n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp61n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp61n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp61;

 Savepoint sp62;
   Savepoint sp62n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp62n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp62n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp62n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp62n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp62n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp62n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp62;

 Savepoint sp63;
     Insert into nested_subtx_t1 values(63, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp63n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp63n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp63n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp63n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp63n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp63n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp63n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp63;

 Savepoint sp64;
   Savepoint sp64n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp64n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp64n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp64n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp64n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp64n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp64n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp64;

 Savepoint sp65;
     Insert into nested_subtx_t1 values(65, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp65n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp65n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp65n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp65n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp65n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp65n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp65n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp65;

 Savepoint sp66;
   Savepoint sp66n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp66n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp66n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp66n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp66n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp66n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp66n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp66;

 Savepoint sp67;
     Insert into nested_subtx_t1 values(67, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp67n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp67n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp67n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp67n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp67n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp67n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp67n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp67;

 Savepoint sp68;
   Savepoint sp68n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp68n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp68n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp68n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp68n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp68n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp68n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp68;

 Savepoint sp69;
     Insert into nested_subtx_t1 values(69, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp69n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp69n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp69n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp69n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp69n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp69n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp69n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp69;

 Savepoint sp70;
   Savepoint sp70n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp70n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp70n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp70n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp70n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp70n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp70n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp70;

 Savepoint sp71;
     Insert into nested_subtx_t1 values(71, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp71n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp71n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp71n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp71n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp71n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp71n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp71n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp71;

 Savepoint sp72;
   Savepoint sp72n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp72n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp72n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp72n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp72n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp72n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp72n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp72;

 Savepoint sp73;
     Insert into nested_subtx_t1 values(73, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp73n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp73n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp73n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp73n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp73n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp73n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp73n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp73;

 Savepoint sp74;
   Savepoint sp74n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp74n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp74n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp74n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp74n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp74n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp74n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp74;

 Savepoint sp75;
     Insert into nested_subtx_t1 values(75, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp75n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp75n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp75n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp75n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp75n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp75n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp75n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp75;

 Savepoint sp76;
   Savepoint sp76n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp76n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp76n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp76n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp76n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp76n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp76n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp76;

 Savepoint sp77;
     Insert into nested_subtx_t1 values(77, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp77n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp77n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp77n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp77n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp77n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp77n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp77n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp77;

 Savepoint sp78;
   Savepoint sp78n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp78n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp78n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp78n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp78n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp78n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp78n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp78;

 Savepoint sp79;
     Insert into nested_subtx_t1 values(79, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp79n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp79n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp79n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp79n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp79n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp79n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp79n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp79;

 Savepoint sp80;
   Savepoint sp80n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp80n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp80n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp80n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp80n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp80n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp80n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp80;

 Savepoint sp81;
     Insert into nested_subtx_t1 values(81, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp81n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp81n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp81n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp81n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp81n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp81n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp81n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp81;

 Savepoint sp82;
   Savepoint sp82n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp82n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp82n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp82n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp82n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp82n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp82n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp82;

 Savepoint sp83;
     Insert into nested_subtx_t1 values(83, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp83n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp83n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp83n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp83n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp83n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp83n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp83n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp83;

 Savepoint sp84;
   Savepoint sp84n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp84n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp84n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp84n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp84n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp84n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp84n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp84;

 Savepoint sp85;
     Insert into nested_subtx_t1 values(85, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp85n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp85n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp85n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp85n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp85n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp85n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp85n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp85;

 Savepoint sp86;
   Savepoint sp86n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp86n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp86n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp86n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp86n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp86n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp86n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp86;

 Savepoint sp87;
     Insert into nested_subtx_t1 values(87, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp87n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp87n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp87n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp87n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp87n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp87n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp87n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp87;

 Savepoint sp88;
   Savepoint sp88n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp88n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp88n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp88n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp88n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp88n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp88n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp88;

 Savepoint sp89;
     Insert into nested_subtx_t1 values(89, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp89n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp89n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp89n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp89n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp89n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp89n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp89n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp89;

 Savepoint sp90;
   Savepoint sp90n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp90n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp90n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp90n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp90n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp90n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp90n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp90;

 Savepoint sp91;
     Insert into nested_subtx_t1 values(91, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp91n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp91n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp91n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp91n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp91n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp91n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp91n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp91;

 Savepoint sp92;
   Savepoint sp92n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp92n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp92n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp92n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp92n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp92n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp92n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp92;

 Savepoint sp93;
     Insert into nested_subtx_t1 values(93, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp93n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp93n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp93n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp93n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp93n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp93n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp93n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp93;

 Savepoint sp94;
   Savepoint sp94n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp94n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp94n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp94n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp94n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp94n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp94n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp94;

 Savepoint sp95;
     Insert into nested_subtx_t1 values(95, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp95n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp95n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp95n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp95n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp95n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp95n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp95n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp95;

 Savepoint sp96;
   Savepoint sp96n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp96n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp96n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp96n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp96n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp96n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp96n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp96;

 Savepoint sp97;
     Insert into nested_subtx_t1 values(97, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp97n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp97n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp97n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp97n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp97n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp97n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp97n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp97;

 Savepoint sp98;
   Savepoint sp98n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp98n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp98n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp98n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp98n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp98n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp98n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp98;

 Savepoint sp99;
     Insert into nested_subtx_t1 values(99, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp99n1;
Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp99n2;
Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp99n3;
Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Savepoint sp99n4;
Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp99n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp99n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp99n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Release Savepoint sp99;

 Savepoint sp100;
   Savepoint sp100n1;
     Insert into nested_subtx_t1 values(1, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp100n2;
     Insert into nested_subtx_t1 values(2, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp100n3;
     Insert into nested_subtx_t1 values(3, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
   Savepoint sp100n4;
     Insert into nested_subtx_t1 values(4, 'sub- and nested transactions','a','variable string','2-2-2012',5,'2012-10-10 10:23:54', '2011-10-19 10:23:54+02');
Update nested_subtx_t1 set a2 = 'New Value' where a1 =4;
   Release Savepoint sp100n4;

Select a2 from nested_subtx_t1 where a1 = 4;
Delete from nested_subtx_t2 where b1>0 ;
   Rollback to sp100n3;

Update nested_subtx_t1 set a2 = 'New Value' where a1 =2;
   Release Savepoint sp100n2;

Select count(*) from nested_subtx_t1;
Select count(*) from nested_subtx_t2;
 Rollback to sp100;

Commit;
