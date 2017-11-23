--
-- Setup for bgworker.sql tests
-- 

-- We are setting shared_preload_libraries to "bgworker_example.so" and
-- restart database, then use bgworker.sql to verify bgworker processes
-- are correctly launched 

-- start_ignore 
\! cp $MASTER_DATA_DIRECTORY/postgresql.conf $MASTER_DATA_DIRECTORY/postgresql.conf.bgworkertest.bak
\! gpconfig -c shared_preload_libraries -v "bgworker_example"
\! PGDATESTYLE="" gpstop -rai
-- end_ignore
