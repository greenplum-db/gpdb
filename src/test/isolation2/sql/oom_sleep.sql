-- start_ignore
1&: SELECT COUNT(1) from lineitem, (SELECT pg_sleep(50)) AS t;
2&: SELECT COUNT(1) from lineitem, (SELECT pg_sleep(50)) AS t;
1<:
2<:
-- end_ignore
