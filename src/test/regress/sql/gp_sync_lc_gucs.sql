-- This case test that if lc related GUCs are synchronized
-- between QD and QEs.

CREATE TABLE test_lc(c1 int8, c2 date) DISTRIBUTED BY (c1);
CREATE OR REPLACE FUNCTION public.segment_setting(guc text)
    RETURNS SETOF text EXECUTE ON ALL SEGMENTS AS $$
    BEGIN RETURN NEXT pg_catalog.current_setting(guc); END
    $$ LANGUAGE plpgsql;

INSERT INTO test_lc values ('4567890123456789', '2022-08-01');
INSERT INTO test_lc values ('-4567890123456789', '2022-09-01');

-- Test if lc_monetary is synced
SHOW lc_monetary;
SELECT to_char(c1, 'L9999999999999999.000')  FROM test_lc;

SET lc_monetary = 'en_US.utf8';
SELECT to_char(c1, 'L9999999999999999.000')  FROM test_lc;

-- If the QE processes are exited for whatever the reason,
-- QD should sync the lc_monetary to the newly created QEs.
SELECT pg_terminate_backend(pid) FROM gp_dist_random('pg_stat_activity') WHERE sess_id
 in (SELECT sess_id from pg_stat_activity WHERE pid in (SELECT pg_backend_pid())) ; 
-- Should output the results given lc_monetary = 'en_US.utf8'
SELECT to_char(c1, 'L9999999999999999.000')  FROM test_lc;

RESET lc_monetary;


-- Test if lc_time is synced
SHOW lc_time;
SELECT to_char(c2, 'DD TMMON YYYY') FROM test_lc;

SET lc_time = 'en_US.utf8';
-- Since 'C' and 'en_US.utf8' time formatting will output the same result, and in
-- some environments, we don't know which kind of locale it supports. So we just
-- use segment_setting to checking the setting of lc_time on QEs.
SELECT segment_setting('lc_time');

-- If the QE processes are exited for whatever the reason,
-- QD should sync the lc_time to the newly created QEs.
SELECT pg_terminate_backend(pid) FROM gp_dist_random('pg_stat_activity') WHERE sess_id
 in (SELECT sess_id from pg_stat_activity WHERE pid in (SELECT pg_backend_pid())) ; 
SELECT segment_setting('lc_time');

DROP FUNCTION public.segment_setting(guc text);
DROP TABLE test_lc;
