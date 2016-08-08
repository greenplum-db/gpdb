Create table subt_tab_distby_ao10 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed by (i);
Create table subt_tab_distby_ao102 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed by (i);
Create table subt_tab_dist_random_ao10 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed randomly;
Create table subt_tab_dist_random_ao102 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed randomly;
Create table subt_tab_const_ao10 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone constraint chk1 CHECK(i>0)) with (appendonly=true) distributed randomly;
Create table subt_tab_const_ao102 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone constraint chk1 CHECK(i>0)) with (appendonly=true) distributed randomly;
Create table subt_ctas_tab_ao10 as select * from subt_for_ctas_ao;
Create table subt_ctas_tab_ao102 as select * from subt_for_ctas_ao;

