-- @gucs gp_create_table_random_default_distribution=off
create table ao_part01
(
distcol int, ptcol int, subptcol int ) with(appendonly=true)
distributed by (distcol) partition by range (ptcol)
subpartition by list (subptcol) subpartition template (
default subpartition subothers, subpartition sub1 values(1,2,3), subpartition sub2 values(4,5,6) )
( default partition others, start (1) end (10) inclusive every (5) );

insert into ao_part01 values(generate_series(1,5), generate_series(1,15), generate_series(1,7));

create table ao_part02
 (
 col1 bigint, col2 date, col3 text, col4 int) with(appendonly=true)
 distributed randomly  partition by range(col2)
 subpartition by list (col3)
 subpartition template ( default subpartition subothers, subpartition sub1 values ('one'), subpartition sub2 values ('two'))    
 (default partition others, start(date '2008-01-01') end(date '2008-04-30') every(interval '1 month'));

insert into ao_part02(col1, col2, col3) values (1, '2008-01-02', 'one'), (2, '2008-01-02', 'two'), (3, '2008-01-02', 'three');
insert into ao_part02(col1, col2, col3) values (1, '2008-02-03', 'two'), (2, '2008-02-03', 'one'), (3, '2008-02-03', 'three');
insert into ao_part02(col1, col2, col3)  values(1, '2008-03-04', 'three'),(1, '2008-04-04', 'one'), (2, '2008-03-04', 'two') ;
insert into ao_part02(col1, col2, col3) values (1, '2008-04-05', 'one'), (2, '2008-04-05', 'two'), (3, '2008-04-05', 'three');
insert into ao_part02(col1, col2, col3) values (1, '2008-05-06', 'one'), (2, '2008-05-06', 'two'), (3, '2008-05-06', 'three');


