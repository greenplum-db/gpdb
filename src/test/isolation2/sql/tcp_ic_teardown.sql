-- Test ensuring that we perform a timed wait inside the TCP interconnect
-- teardown on the motion sender side, for the final response from the motion
-- receiver(s).

CREATE FUNCTION set_gp_ic_type(ic_type text)
    RETURNS VOID as $$
import os
cmd = 'gpconfig -c gp_interconnect_type -v %s' % ic_type
if os.system(cmd) is not 0:
    plpy.error('Setting gp_interconnect_type to %s failed' % ic_type)
$$ LANGUAGE plpythonu;

CREATE TABLE tcp_ic_teardown(i int);
INSERT INTO tcp_ic_teardown SELECT generate_series(1, 5);

-- Save current IC type before we set it to 'tcp', so we can revert it at the
-- end of the test.
-1U: CREATE TABLE saved_ic_type AS SELECT current_setting('gp_interconnect_type') AS ic_type;
-1U: SELECT set_gp_ic_type('tcp');
!\retcode gpstop -au;

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

-- Revert IC type
-1U: SELECT set_gp_ic_type(ic_type) FROM saved_ic_type;
!\retcode gpstop -au;
-1U: DROP TABLE saved_ic_type;
