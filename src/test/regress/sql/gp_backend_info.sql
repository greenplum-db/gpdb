-- Tests for the gp_backend_info() function.

-- At first there should be no segment backends; we haven't performed any
-- queries yet. There should only be a QD backend
SELECT COUNT(*) = 1 FROM gp_backend_info();

SELECT type, content FROM gp_backend_info();
--
-- Spin up the writer gang.
--

CREATE TEMPORARY TABLE temp();

--start_ignore
-- Help debugging by printing the results. Since most contents will be different
-- on every machine, we do the actual verification below.
SELECT * from gp_backend_info();
--end_ignore

-- Now we should have as many backends as primaries +1 QD, and all primaries
-- backend should be marked as writers
SELECT COUNT(*) AS num_primaries FROM gp_segment_configuration
    WHERE content >= 0 AND role = 'p'
\gset
SELECT COUNT(*) = :num_primaries +1 FROM gp_backend_info();
SELECT COUNT(*) = :num_primaries FROM gp_backend_info() WHERE type = 'w';
SELECT COUNT(*) = 1 FROM gp_backend_info() WHERE type = 'Q';

-- All IDs and PIDs should be distinct.
SELECT COUNT(DISTINCT id) = :num_primaries +1 FROM gp_backend_info();
SELECT COUNT(DISTINCT content) = :num_primaries +1 FROM gp_backend_info();
SELECT COUNT(DISTINCT pid) = :num_primaries +1 FROM gp_backend_info();

--
-- Spin up a parallel reader gang.
--

CREATE TEMPORARY TABLE temp2();
SELECT * FROM temp JOIN (SELECT * FROM temp2) temp2 ON (temp = temp2);

--start_ignore
-- Debugging helper (see above).
SELECT * from gp_backend_info();
--end_ignore

-- Now we should have double the number of backends; the new ones should be
-- readers.
SELECT COUNT(*) = (:num_primaries * 2) +1 FROM gp_backend_info();
SELECT COUNT(*) = :num_primaries FROM gp_backend_info() WHERE type = 'w';
SELECT COUNT(*) = :num_primaries FROM gp_backend_info() WHERE type = 'r';
SELECT COUNT(*) = 1 FROM gp_backend_info() WHERE type = 'Q';

-- IDs and PIDs should still be distinct.
SELECT COUNT(DISTINCT id) = (:num_primaries * 2) +1 FROM gp_backend_info();
SELECT COUNT(DISTINCT pid) = (:num_primaries * 2) +1 FROM gp_backend_info();

-- Content IDs should be there twice (a reader and a writer for each segment).
SELECT COUNT(DISTINCT content) = :num_primaries +1 FROM gp_backend_info();
SELECT COUNT(DISTINCT content) = :num_primaries FROM gp_backend_info()
WHERE content >= 0;
SELECT DISTINCT COUNT(content) FROM gp_backend_info() WHERE content >= 0
GROUP BY content;

--
-- Start up a singleton reader.
--

SELECT * FROM temp JOIN (SELECT oid FROM pg_class) temp2 on (temp = temp2);

--start_ignore
-- Debugging helper (see above).
SELECT * from gp_backend_info();
--end_ignore

-- We should have added only one backend -- the singleton reader on the master.
SELECT COUNT(*) = (:num_primaries * 2 + 2) FROM gp_backend_info();
SELECT COUNT(*) = :num_primaries FROM gp_backend_info() WHERE type = 'w';
SELECT COUNT(*) = :num_primaries FROM gp_backend_info() WHERE type = 'r';
SELECT COUNT(*) = 1 FROM gp_backend_info() WHERE type = 'R' and content = -1;
SELECT COUNT(*) = 1 FROM gp_backend_info() WHERE type = 'Q' and content = -1;

-- IDs and PIDs should still be distinct.
SELECT COUNT(DISTINCT id) = (:num_primaries * 2 + 2) FROM gp_backend_info();
SELECT COUNT(DISTINCT pid) = (:num_primaries * 2 + 2) FROM gp_backend_info();
