set allow_system_table_mods=true;
create table foo(i int primary key);
update gp_distribution_policy  set distkey='' where localoid='foo'::regclass::oid;
