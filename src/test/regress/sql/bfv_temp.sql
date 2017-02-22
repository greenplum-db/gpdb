-- MPP-24237
-- Security definer function causes temp table not to be dropped due to pg_toast access privileges

CREATE or replace FUNCTION sec_definer_create_test() RETURNS void AS $$
BEGIN
  RAISE NOTICE 'Creating table';
  execute 'create temporary table wmt_toast_issue_temp (name varchar, address varchar) distributed randomly';
  RAISE NOTICE 'Table created';
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

create role sec_definer_role with login ;

grant execute on function sec_definer_create_test() to sec_definer_role;

set role sec_definer_role;

select sec_definer_create_test() ;

-- There should be one temp table, the one we created (we assume that there are no
-- other backends using temp tables running at the same time).
select count(*) from pg_tables where schemaname like 'pg_temp%';

-- Disconnect and reconnect.
\c regression

-- It can take a while for the old backend to finish cleaning up the
-- temp tables.
select pg_sleep(2);

-- Check that the temporary table was dropped at disconnect.
select * from pg_tables where schemaname like 'pg_temp%';

-- Clean up
reset role;
drop function public.sec_definer_create_test();
drop role sec_definer_role;

