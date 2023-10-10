-- Test case for the scenario which ic-proxy peer listener port has been occupied

-- start_matchsubs
-- m/ic_tcp.c:\d+/
-- s/ic_tcp.c:\d+/ic_tcp.c:LINE/
-- end_matchsubs

1:create table PR_16438 (i int);
1:insert into PR_16438 select generate_series(1,100);
1q:

-- get one port and occupy it (start_py_httpserver.sh), then restart cluster
-- start_ignore
! ic_proxy_port=`psql postgres -Atc "show gp_interconnect_proxy_addresses;" | awk -F ',' '{print $1}' | awk -F ':' '{print $4}'` && gpstop -ai && ./script/start_py_httpserver.sh $ic_proxy_port;
! sleep 2 && gpstart -a;
-- end_ignore

-- execute a query (should failed)
2&:select count(*) from PR_16438;
2<:

-- kill the script to release port and execute query again (should successfully)
-- start_ignore
! ps aux | grep http.server | grep -v grep | awk '{print $2}' | xargs kill;
! sleep 2;
-- end_ignore
3:select count(*) from PR_16438;
3:drop table PR_16438;
