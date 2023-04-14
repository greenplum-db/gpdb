-- Test ensuring that we perform a timed wait inside the TCP interconnect
-- teardown on the motion sender side, for the final response from the motion
-- receiver(s).
CREATE TABLE tcp_ic_teardown(i int);
INSERT INTO tcp_ic_teardown SELECT generate_series(1, 5);

SELECT gp_inject_fault('waitOnOutbound', 'suspend', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
SELECT gp_inject_fault('doSendStopMessageTCP', 'suspend', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

1: SET gp_interconnect_transmit_timeout TO '3s';
-- Use a LIMIT to squelch the motion node in order to send a 'stop' message.
1&: SELECT * FROM tcp_ic_teardown LIMIT 1;

-- Ensure that we have suspended the QD's gather motion receiver at the point
-- before it sends out the 'stop' message and have reached the point just prior
-- to starting the timed wait during TCP teardown on one of the motion senders.
SELECT gp_wait_until_triggered_fault('waitOnOutbound', 1, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
SELECT gp_wait_until_triggered_fault('doSendStopMessageTCP', 1, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;

-- Let the timed wait proceed on the sender side.
SELECT gp_inject_fault('waitOnOutbound', 'reset', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;

!\retcode sleep 6;

SELECT gp_inject_fault('doSendStopMessageTCP', 'reset', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = -1;
-- After 6s have elapsed (enough to have covered the timed wait of 3s, we should
-- have consequently ERRORed out on the motion sender side)
1<:
