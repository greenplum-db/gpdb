create or replace function verify(varchar) returns bigint as
$$
select count(foo.*) from(
                    select oid, rulename, ev_class::regclass
                    from pg_rewrite
                    where ev_class=$1::regclass
                    union
                    select oid, rulename, ev_class::regclass
                    from gp_dist_random('pg_rewrite')
                    where ev_class=$1::regclass
                    ) foo;
$$ language sql;

-- copied from existing cdbfast tests:
-- (//cdbfast/Release-4_3-branch/oid_inconsistency/...)

drop view if exists bug cascade;
create view bug as select 1;
select verify('bug');

DROP TABLE IF EXISTS foo_ao cascade;
DROP TABLE IF EXISTS bar_ao cascade;
CREATE table foo_ao (a int) with (appendonly=true) distributed by (a);
CREATE table bar_ao (a int) distributed by (a);
CREATE rule one as on insert to bar_ao do instead update foo_ao set a=1;
select verify('bar_ao');

DROP TABLE IF EXISTS foo2 cascade;
DROP TABLE IF EXISTS bar2 cascade;
CREATE table foo2 (a int) distributed by (a);
CREATE table bar2 (a int) distributed by (a);
CREATE rule two as on insert to bar2 do instead insert into foo2(a) values(1);
select verify('bar2');

DROP VIEW IF EXISTS tt1 cascade;
DROP TABLE IF EXISTS tt2 cascade;
CREATE table tt1 (a int) distributed by (a);
CREATE table tt2 (a int) distributed by (a);
CREATE rule "_RETURN" as on select to tt1
        do instead select * from tt2;
select verify('tt1');
