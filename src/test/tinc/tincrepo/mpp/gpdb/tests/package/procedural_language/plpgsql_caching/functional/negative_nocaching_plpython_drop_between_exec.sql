-- start_ignore
drop table if exists aopython cascade;
drop function if exists pythonfunc();
-- end_ignore

create table aopython(a int, b int, c text) with (appendonly=true) distributed by (a);

insert into aopython(a, b, c) values (1, 1, 'test11');
insert into aopython(a, b, c) values (1, 2, 'test12');
insert into aopython(a, b, c) values (2, 1, 'test21');
insert into aopython(a, b, c) values (2, 2, 'test22');

create or replace function pythonfunc() returns integer as
$$
    rv = plpy.execute("SELECT count(*) as count from aopython WHERE a = 1");
    return rv[0]["count"];
    
$$ language plpythonu;

select pythonfunc();

drop table aopython;

create table aopython(a int, b int, c text) with (appendonly=true) distributed by (a);

select pythonfunc();

