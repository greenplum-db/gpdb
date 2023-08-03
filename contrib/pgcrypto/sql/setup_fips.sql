-- Setup for fips.sql test

-- We are setting shared_preload_libraries so that we can set the extension GUC
-- 'pgcrypto.fips' immediately after creating the extension on master.

-- start_ignore
\! gpconfig -c shared_preload_libraries -v "$(gpconfig -s shared_preload_libraries | grep value: | head -n 1 | awk -F ': ' '{print $2}'),pgcrypto.so"
\! gpstop -air
-- end_ignore
