select count(*) from pg_class where relname like 'subt_tab_distby_rel_ao%';

--Create table should fail saying a table already exists
Create table subt_tab_distby_rel_ao10 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed by (i);
Create table subt_tab_distby_rel_ao102 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed by (i);

