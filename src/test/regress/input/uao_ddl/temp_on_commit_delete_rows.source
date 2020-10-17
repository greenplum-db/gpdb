create schema temp_on_commit_delete_rows_@amname@;
set search_path="$user",temp_on_commit_delete_rows_@amname@,public;
SET default_table_access_method=@amname@;
--
-- Test ON COMMIT DELETE ROWS action against AO/AOCS tables.
--

create function nonempty_segno(oid) returns setof int as 
$$
-- Input to this function must be oid and not table name.  A distinct
-- query fails to find the temp table in the right namespace
-- otherwise.
select segno from gp_toolkit.__gp_@aoseg@($1::regclass::text)
 where tupcount > 0;
$$ language sql;

begin;
create temp table temp_delrows(a int, b int)
 on commit delete rows distributed by(a);
insert into temp_delrows select i, i from generate_series(1,20)i;
create index idelrows on temp_delrows using bitmap(a);
update temp_delrows set b = -a;
-- Aoseg table should report at least one non-empty segfile.
select distinct nonempty_segno('temp_delrows'::regclass);
select distinct nonempty_segno('temp_delrows'::regclass)
 from gp_dist_random('gp_id');
commit;

select count(*) = 0 as passed from temp_delrows;

-- All segfiles must be empty after commit.
select nonempty_segno('temp_delrows'::regclass);
select nonempty_segno('temp_delrows'::regclass)
 from gp_dist_random('gp_id');

-- DML after delete rows action.
begin;
insert into temp_delrows select i, i from generate_series(1,50)i;
delete from temp_delrows where a > 25;
select count(*) = 25 as passed from temp_delrows;

-- Abort should leave no tuples in the table.
abort;

select count(*) = 0 as passed from temp_delrows;

-- No begin/end block, each insert is a separate transaction
insert into temp_delrows values (1,1), (2,2), (3,3);
select count(*) = 0 as passed from temp_delrows;
insert into temp_delrows select i,i from generate_series(1,10)i;
select count(*) = 0 as passed from temp_delrows;

-- All segfiles must be empty after commit.
select nonempty_segno('temp_delrows'::regclass);
select nonempty_segno('temp_delrows'::regclass)
 from gp_dist_random('gp_id');

