-- @product_version gpdb: [4.3.2-4.3.98]
drop table if exists ctas_src;
drop table if exists ctas_dst;

create table ctas_src (domain integer, class integer, attr text, value integer) distributed by (domain);
insert into ctas_src values(1, 1, 'A', 1);
insert into ctas_src values(2, 1, 'A', 0);
insert into ctas_src values(3, 0, 'B', 1);

-- MPP-2859
create table ctas_dst as 
SELECT attr, class, (select count(distinct class) from ctas_src) as dclass FROM ctas_src GROUP BY attr, class distributed by (attr);

drop table ctas_dst;

create table ctas_dst as 
SELECT attr, class, (select max(class) from ctas_src) as maxclass FROM ctas_src GROUP BY attr, class distributed by (attr);

drop table ctas_dst;

create table ctas_dst as 
SELECT attr, class, (select count(distinct class) from ctas_src) as dclass, (select max(class) from ctas_src) as maxclass, (select min(class) from ctas_src) as minclass FROM ctas_src GROUP BY attr, class distributed by (attr);

-- MPP-4298: "unknown" datatypes.
drop table if exists ctas_foo;
drop table if exists ctas_bar;
drop table if exists ctas_baz;

create table ctas_foo as select * from generate_series(1, 100);
create table ctas_bar as select a.generate_series as a, b.generate_series as b from ctas_foo a, ctas_foo b;

create table ctas_baz as select 'delete me' as action, * from ctas_bar;
-- "action" has no type.
\d ctas_baz
select action, b from ctas_baz order by 1,2 limit 5;
select action, b from ctas_baz order by 2 limit 5;
select action::text, b from ctas_baz order by 1,2 limit 5;

alter table ctas_baz alter column action type text;
\d ctas_baz
select action, b from ctas_baz order by 1,2 limit 5;
select action, b from ctas_baz order by 2 limit 5;
select action::text, b from ctas_baz order by 1,2 limit 5;

