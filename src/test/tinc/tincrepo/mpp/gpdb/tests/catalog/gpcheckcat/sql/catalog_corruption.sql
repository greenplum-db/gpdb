-- This file will run only on segments. Any DML or DDL will introduce corruption

set allow_system_table_mods='DML';

-- This will introduce missing_extraneous test failure.
delete from pg_type where typname='t1';
create table foo as select * from generate_series(1, 1000) a;
delete from pg_appendonly where relid='ao_t1'::regclass;

-- This will introduce inconsistent test failure.
update pg_class set relhasrules = true where relname = 't1';
