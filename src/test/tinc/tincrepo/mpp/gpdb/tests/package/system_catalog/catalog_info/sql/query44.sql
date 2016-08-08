-- MPP-2672
-- start_ignore
explain analyze select oid, 'foo'::text from (select oid from pg_class where exists(select reltoastrelid from pg_class)) as foo;

explain analyze select oid, 'foo'::text from (select oid from pg_class where not exists(select reltoastrelid from pg_class)) as foo;
-- end_ignore

select 1;

-- MPP-2655
-- start_ignore
select oid, 'foo'::text from (select oid from pg_class where not exists(select oid from pg_class except select reltoastrelid from pg_class)) as foo;

select oid, 'foo'::text from (select oid from pg_class where not exists(select oid from pg_class except select reltoastrelid from pg_class)) as foo;
-- end_ignore

select 1;
