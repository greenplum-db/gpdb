CREATE EXTENSION pg_buffercache;

select count(*) = (select setting::bigint
                   from pg_settings
                   where name = 'shared_buffers')
from pg_buffercache;

SELECT count(*) = (select setting::bigint
                   from pg_settings
                   where name = 'shared_buffers') *
                   (select count(*) from gp_segment_configuration where role='p')
                   as buffers
FROM gp_buffercache;

-- Check that the functions / views can't be accessed by default.
CREATE ROLE buffercache_test;
SET ROLE buffercache_test;
SELECT * FROM pg_buffercache;
SELECT * FROM pg_buffercache_pages() AS p (wrong int);
SELECT * FROM gp_buffercache;
RESET ROLE;
DROP ROLE buffercache_test;