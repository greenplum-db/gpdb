-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

-- Update on an AO table
Update uao_table1 set text_col= 'new_0_zero' where bigint_col=0;

-- Delete on an AO table
Delete from uao_table2 where col_text='0_zero';

-- Update on an AO table in non-public schema
Update uaoschema1.uao_table3 set stud_name='wen' where stud_id=1;

-- Insert into a UAO table
Insert into uao_table6 values ('3_zero',3);
Insert into uao_table8 values ('4_zero', 4, '4_zero', 4, 4, 4, '{4}', 4, 4, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',4);

-- Insert into a UAO table in non-public schema
Insert into uaoschema1.uao_table7 values('4_four',4);

-- Update on a compressed table
Update uao_compr01 set col_text='new_0_zero' where col_numeric=0;

-- Insert  on a UAO compressed table
Insert into uao_compr02 values ('5_two',5);

-- Update on an AO partition table
Update uao_part01 set distcol=20 where ptcol=1;

-- Delete on a AO partition table
Delete from uao_part02 where b=2;

-- Insert on a UAO partition table
insert into uao_part04 values(2,2,2,2);

-- Add a partition to a UAO table
Alter table uao_part05 add partition new_p start(15) end(20) with(appendonly=true);

--Update with non partition key in where clause
update uao_part08 set amt=9.3 where id=1;

--Update with partition key and a non-partition key in the where clause such that it will update a tuple 
update uao_part09 set amt=4.2 where sdate='2013-08-12' and id=3;

--Delete with non-partition key in where cluase
delete from uao_part10 where id=6;

--Delete with partiion key and non-partition key in where cluase such that tuples will be delted 
delete from uao_part11 where sdate='2013-07-21' and id<3;

--Update with non subpartition key in where clause
update uao_part12 set amt=22.3 where id=1;

--Update with subpartition key and a non-subpartition key in the where clause such that it will update a tuple 
update uao_part13 set  amt=1.2 where id=2 and day='thu';

--Update with subpartition key and non-subpartition key in the where clause such that no tuple will be updated 
update uao_part14 set amt=6.7 where id=3 and day='fri';

--Delete with non-subpartition key in where cluase 
delete from uao_part15 where id=4;

--Delete with subpartiion key and non-subpartition key in where cluase such that tuples will be delted 
delete from uao_part16 where day='wed' and id=4;

--Delete with subpartiion key and non-subpartition key in where cluase such that no tuples will be delted 
delete from uao_part17 where id=3 and day='fri';

-- Delete with sub-partitionkey in where clause
delete from uao_part18 where day='tue';

