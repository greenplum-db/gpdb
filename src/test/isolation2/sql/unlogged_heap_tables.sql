-- expect: create table succeeds
create unlogged table unlogged_heap_table_managers (
	id int,
	name text
) distributed by (id);


-- expect: insert/update/select works
insert into unlogged_heap_table_managers values (1, 'Joe');
insert into unlogged_heap_table_managers values (2, 'Jane');
update unlogged_heap_table_managers set name = 'Susan' where id = 2;
select * from unlogged_heap_table_managers order by id;

-- ensure that operations have been written to the xlog before testing recovery
1: checkpoint;

-- force an unclean stop and recovery:
! gpstop -rai > /tmp/unlogged_heap_tables_forced_restart.log;

-- expect inserts/updates are truncated after crash recovery
2: select * from unlogged_heap_table_managers;


-- expect: insert/update/select works
3: insert into unlogged_heap_table_managers values (1, 'Joe');
3: insert into unlogged_heap_table_managers values (2, 'Jane');
3: update unlogged_heap_table_managers set name = 'Susan' where id = 2;
3: select * from unlogged_heap_table_managers order by id;

-- force a clean stop and recovery:
! gpstop -ra > /tmp/unlogged_heap_tables_clean_restart.log;

-- expect: inserts/updates are persisted
4: select * from unlogged_heap_table_managers order by id;

-- expect: drop table succeeds
5: drop table unlogged_heap_table_managers;

