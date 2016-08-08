DROP table if exists rewrite_table;

CREATE table rewrite_table(i int, t text);

INSERT into rewrite_table values(generate_series(1,10),'jhsdgfjsdhgfds');

CREATE view view1 as select * from rewrite_table where i <5;

select oid, rulename, ev_class::regclass from pg_rewrite where ev_class='view1'::regclass;
select oid, rulename, ev_class::regclass from gp_dist_random('pg_rewrite') where ev_class='view1'::regclass;

