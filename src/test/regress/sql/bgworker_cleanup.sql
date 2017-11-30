--
-- Clean up for bgworker.sql tests
-- 

-- start_ignore
\! cp $MASTER_DATA_DIRECTORY/postgresql.conf.bgworkertest.bak $MASTER_DATA_DIRECTORY/postgresql.conf
\! rm -f $MASTER_DATA_DIRECTORY/postgresql.conf.bgworkertest.bak
\! PGDATESTYLE="" gpstop -rai
-- end_ignore
