-- start_ignore
drop table if exists aopython cascade;
drop function if exists pythonfunc_prep();
drop function if exists pythonfunc_exec(int);
-- end_ignore

create table aopython(a int, b int, c text) with (appendonly=true) distributed by (a);

insert into aopython(a, b, c) values (1, 1, 'test11');
insert into aopython(a, b, c) values (1, 2, 'test12');
insert into aopython(a, b, c) values (2, 1, 'test21');
insert into aopython(a, b, c) values (2, 2, 'test22');

create or replace function pythonfunc_prep() returns text as
$$
    SD["myplan"] = plpy.prepare("SELECT count(*) as count FROM aopython WHERE a = $1", [ "int" ]);
$$ language plpythonu;

create or replace function pythonfunc_exec(id int) returns integer as
$$
    if SD.has_key("myplan"):
        plpy.execute(SD["myplan"], [ id ], 1);
        return 0;
    else:
        SD["myplan"] = plpy.prepare("SELECT count(*) as count FROM aopython WHERE a = $1", [ "int" ]);
        rv = plpy.execute(SD["myplan"], [ id ], 1);
        return rv[0]["count"];
$$ language plpythonu;

select pythonfunc_prep();

select pythonfunc_exec(1);

drop table aopython;

select pythonfunc_exec(1);
