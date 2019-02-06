set allow_system_table_mods=true;
create table foo(i int primary key);
update pg_constraint set conkey='{}' where conname = 'foo_pkey';
