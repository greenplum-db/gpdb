-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

-- Delete on a updated AO table
Delete from uco_table1 where bigint_col=1;

-- Update on a deleted AO table
Update uco_table2 set col_text='new_1_zero' where col_numeric=1;

-- Add a column to an updated AO table
Alter table ucoschema1.uco_table3 add column stud_age int default 1;

-- Truncate a UAO table
Truncate table uco_table9;

-- Delete on updated compr table
Delete from uco_compr01 where col_numeric=1;

-- Truncate on a partition table
Truncate table uco_part03;

-- Delete on a updated partition table
Delete from uco_part01 where ptcol=2;

-- Update on a deleted partition table
Update uco_part02 set d =10 where c=3;

--Update with partition key and non-partition key in the where clause such that no tuple will be updated 
update uco_part09 set amt=2.5 where sdate='2013-07-21' and id=4;

-- Delete with non-partition key in where clause with no actual tuples deleted 
delete from uco_part10 where id=6;

--Delete with partiion key and non-partition key in where cluase such that no tuples will be delted 
delete from uco_part08 where sdate='2013-07-21' and id=6;

-- Delete with partiion key in where cluase such that no tuples will be delted
delete from uco_part11 where sdate='2013-07-21';

--Delete with non-subpartition key in where clause with no actual tuples deleted 
delete from uco_part15 where id=4;

--Update with subpartiion key in where cluase such that no tuples will be updated
update uco_part16 set amt=74.3 where day='wed';

--Delete with subpartiion key in where cluase such that no tuples will be delted 
delete from uco_part18 where day='tue';

--Delete with one partiion key and a sub-partition key in where cluase such that all partiions of one partition and one sub-partition of another is updated 
delete from uco_part12 where sdate='2013-09-15' or day='mon';

--Delete with partiion key and sub-partitiion key in where cluase such that no tuples will be delted
delete from uco_part13 where sdate='2013-08-12' and day='tue';

