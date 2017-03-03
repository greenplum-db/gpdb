-- @gucs gp_create_table_random_default_distribution=off
Insert into ib_heap_01 values(1,2,3,'data');

Insert into ib_heap_schema.ib_heap_01 values(1,2,3,'data');

Insert into ib_heap_part01 values(1,generate_series(1,3), generate_series(1,6), 'data');

Insert into ib_ao_01 values(1,2,3,'data');

Insert into ib_ao_02 values(1,2,3,'data');

Insert into ib_ao_03 values(1, 2, 3, 'data');
Insert into ib_ao_03 values(1, 3, 5, 'data');

begin;
Insert into ib_ao_03 values(1, 3, 7, 'data');
abort;

Insert into ib_ao_schema.ib_ao_01 values(1,2,3,'data');

Insert into ib_co_01 values(1,2,3,'data');

Insert into ib_co_schema.ib_co_01 values(1,2,3,'data');

Insert into ib_co_02 values(1,2,3,'data');

begin;
Insert into ib_co_03 values(1, 2, 5, 'data');
savepoint s1;
Insert into ib_co_03 values(1, 2, 4, 'data');
rollback to s1;
Insert into ib_co_03 values(1, 3, 7, 'data');
commit;

begin;
Insert into ib_co_04 values(1,2,3,'data');
abort;

Insert into ib_co_05 values(2,2,2,'data');

insert into aoschema1.ao_table22 values(3, 'data3');
insert into aoschema1.ao_table22 values(4, 'data4');

insert into ao_table1 values ('0_zero', 3, '0_zero', 3, 3, 3, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);


Insert into ao_part03 values(1,2,2,4);

Insert into co_part03 values(1,2,2,4);
