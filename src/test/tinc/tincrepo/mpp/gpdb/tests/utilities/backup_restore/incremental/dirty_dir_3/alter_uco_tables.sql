-- @gucs gp_create_table_random_default_distribution=off
-- @product_version gpdb: [4.3.0.0- main]

-- Insert two rows to the table truncated before
insert into uco_table9 values ('5_zero', 5, '5_zero', 5, 5, 5, '{5}', 5, 5, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',5);
insert into uco_table9 values ('6_zero', 6, '6_zero', 6, 6, 6, '{6}', 6, 6, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',6);

-- Insert one row to the partiiton table truncated above
insert into uco_part03 values(10, 2, 4, 5);

--Update with partiion key in where cluase such that no tuples will be updated 
update uco_part11 set amt=5.6 where sdate='2013-07-21';

-- Delete with partiiton key in where cluase
Delete from uco_part10 where sdate='2013-09-15';

Delete from uco_part09 where id<3;

--Update with no where clause
update uco_part08 set amt=23.5;
