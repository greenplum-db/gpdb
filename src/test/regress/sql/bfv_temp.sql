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

-- Check if myTempNamespace is correct in N-Gang query.
create table tn_a(id int) distributed by (id);
create temp table tn_a_tmp(a int) distributed replicated;

insert into tn_a values (1), (2);
insert into tn_a_tmp values(1);

create or replace function fun(sql text, a oid) returns bigint AS 'return plpy.execute(sql).nrows() + a' language plpython3u stable;

create table tn_a_new as with c as (select fun('select * from tn_a_tmp', s.id) from tn_a s) select 1 from c;

drop table tn_a;
drop table tn_a_tmp;
drop table tn_a_new;

-- Check if old gang can accept new temp schema, after temp schema changed on coordinator
\c
create table tn_b_a(id int) distributed by (id);
create table tn_b_b(id int, a_id int) distributed by (id);

insert into tn_b_a values (1), (2);
insert into tn_b_b values (3, 1), (4, 2);
select a.id, b.id from tn_b_a a, tn_b_b b where a.id = b.a_id order by 1, 2;

create temp table tn_b_temp(a int) distributed replicated;
insert into tn_b_temp values(1);

create table tn_b_new as with c as (select fun('select * from tn_b_temp', s.id) from tn_b_b s) select 1 from c;

drop table tn_b_a;
drop table tn_b_b;
drop table tn_b_temp;
drop table tn_b_new;
drop function fun(sql text, a oid);

-- Chek if error out inside UDF, myTempNamespace will roll back
\c
create or replace function errored_udf() returns int[] as 'BEGIN RAISE EXCEPTION ''AAA''; END' language plpgsql;

create table n as select from generate_series(1, 10);
select count(*) from n n1, n n2; -- boot reader gang

create temp table nn as select errored_udf() from n;
create temp table nnn as select * from generate_series(1, 10); -- check if reader do the rollback. should OK
select count(*) from nnn n1, nnn n2; -- check if reader can read temp table. should OK

drop table n;
drop function errored_udf();
