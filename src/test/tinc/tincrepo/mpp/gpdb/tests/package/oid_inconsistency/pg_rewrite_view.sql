DROP view if exists rewrite_view;
DROP table if exists rewrite_table;

CREATE table rewrite_table(i int, t text) distributed by (i);

INSERT into rewrite_table values(generate_series(1,10),'rewrite rules are created on view creation');

CREATE view rewrite_view as select * from rewrite_table where i <5;

select oid, rulename, ev_class::regclass from pg_rewrite where ev_class='rewrite_view'::regclass;
select oid, rulename, ev_class::regclass from gp_dist_random('pg_rewrite') where ev_class='rewrite_view'::regclass;

