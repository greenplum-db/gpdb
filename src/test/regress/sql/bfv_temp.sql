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

-- Remember the name of the temp namespace and temp toast namespace
CREATE TABLE temp_nspnames as
select nsp.nspname as nspname, toastnsp.nspname as toastnspname from pg_class c
inner join pg_namespace nsp on c.relnamespace = nsp.oid
inner join pg_class toastc on toastc.oid = c.reltoastrelid
inner join pg_namespace toastnsp on toastc.relnamespace = toastnsp.oid
where c.oid = 'wmt_toast_issue_temp'::regclass;

-- there should be exactly one temp table with that name.
select count(*) from temp_nspnames;


-- Disconnect and reconnect.
\c regression

-- It can take a while for the old backend to finish cleaning up the
-- temp tables.
select pg_sleep(2);

-- Check that the temp namespaces were dropped altogether.
select nsp.nspname, temp_nspnames.* FROM pg_namespace nsp, temp_nspnames
where nsp.nspname = temp_nspnames.nspname OR nsp.nspname = temp_nspnames.toastnspname;

-- Check that the temporary table was dropped at disconnect. (It really should be
-- gone if the whole namespace is gone, but doesn't hurt to check.)
select * from pg_tables where tablename = 'wmt_toast_issue_temp';


-- Clean up
reset role;
drop table temp_nspnames;
drop function public.sec_definer_create_test();
drop role sec_definer_role;
