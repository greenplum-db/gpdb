set allow_system_table_mods='dml';
create table foo(i int primary key);
update gp_distribution_policy  set attrnums=NULL where localoid='foo'::regclass::oid;
