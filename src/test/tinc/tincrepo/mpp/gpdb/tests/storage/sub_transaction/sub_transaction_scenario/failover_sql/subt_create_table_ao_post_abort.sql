Create table subt_tab_distby_ao10 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed by (i);
Create table subt_tab_distby_ao66 (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with (appendonly=true) distributed by (i);
Create table subt_ctas_tab_ao as select * from subt_for_ctas_ao;


