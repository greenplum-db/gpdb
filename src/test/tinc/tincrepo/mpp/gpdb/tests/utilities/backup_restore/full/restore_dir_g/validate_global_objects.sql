-- @gucs gp_create_table_random_default_distribution=off
select spcname from pg_tablespace where spcname in ('tbsp1', 'tbsp2');
select rolname from pg_roles where rolname in ('bk_user1', 'bk_group1');
