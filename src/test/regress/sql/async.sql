--
-- ASYNC
--

--Should work. Send a valid message via a valid channel name
SELECT pg_notify('notify_async1','sample message1');
SELECT pg_notify('notify_async1','');
SELECT pg_notify('notify_async1',NULL);

-- Should fail. Send a valid message via an invalid channel name
SELECT pg_notify('','sample message1');
SELECT pg_notify(NULL,'sample message1');
SELECT pg_notify('notify_async_channel_name_too_long______________________________','sample_message1');

--Should work. Valid NOTIFY/LISTEN/UNLISTEN commands
NOTIFY notify_async2;
LISTEN notify_async2;
UNLISTEN notify_async2;
UNLISTEN *;

-- start_matchsubs
-- m/Asynchronous notification "notify_async3" received from server process with PID.*/
-- s/Asynchronous notification "notify_async3" received from server process with PID.*/Asynchronous notification "notify_async3" received/
-- end_matchsubs
\c postgres
LISTEN notify_async3;
\! psql postgres -c "notify notify_async3;"
SELECT;
\c -
