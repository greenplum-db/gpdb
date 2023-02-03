-- Test to ensure that peer shutdown doesn't lead to packet loss.
CREATE TABLE ic_proxy_peer_shutdown(i int);
INSERT INTO ic_proxy_peer_shutdown VALUES (1), (2), (3);
-- Will ensure that all peer setup is done.
SELECT * FROM ic_proxy_peer_shutdown;

-- Suspend regular client registration to force placeholder client creation for
-- seg0 <-> seg-1.
SELECT gp_inject_fault('ic_proxy_backend_init_context', 'suspend', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;
SELECT gp_inject_fault_infinite('ic_proxy_client_cached_p2c_pkt', 'skip', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

-- Query involving only seg-1 (receiver) and seg0 (sender) will block.
1&: SELECT * FROM ic_proxy_peer_shutdown WHERE i = 2;

-- Wait until we have created a placeholder client for seg0 <-> seg-1
-- communication in seg-1's proxy, and that placeholder has cached a DATA packet
-- and a BYE packet from seg0.
SELECT gp_wait_until_triggered_fault('ic_proxy_client_cached_p2c_pkt', 2, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

SELECT gp_inject_fault('ic_proxy_client_table_shutdown_by_dbid_end', 'skip', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

-- Now kill any IC proxy process to trigger a peer shutdown event in seg-1's proxy.
!\retcode ps -ef | grep $(psql postgres -Atc "select port from gp_segment_configuration where content=1 and role='p'") | grep "ic proxy" | awk '{print $2}' | xargs kill;

-- Wait until any client cache eviction has finished, resultant from the peer
-- proxy shutdown.
SELECT gp_wait_until_triggered_fault('ic_proxy_client_table_shutdown_by_dbid_end', 1, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

-- We should have handled the peer shutdown event gracefully, not lost any
-- client-cached packets and let the blocked query complete.
-- So, once we resume client processing, the query should be able to complete
-- gracefully.
SELECT gp_inject_fault('ic_proxy_backend_init_context', 'reset', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;
1<:

SELECT gp_inject_fault('ic_proxy_client_cached_p2c_pkt', 'reset', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;
SELECT gp_inject_fault('ic_proxy_client_table_shutdown_by_dbid_end', 'reset', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;
