-- Setup for fips.sql test

-- We are setting shared_preload_libraries so that we can set the extension GUC
-- 'pgcrypto.fips' immediately after creating the extension on master.

-- start_ignore
\! gpconfig -c shared_preload_libraries -v "$(psql -At -c "SELECT array_to_string(array_append(string_to_array(current_setting('shared_preload_libraries'), ','), 'pgcrypto.so'), ',')" postgres)"
\! gpstop -air
-- end_ignore
