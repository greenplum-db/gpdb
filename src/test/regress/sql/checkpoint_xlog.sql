-- start_matchsubs
-- m/PostgreSQL stand-alone backend .*/
-- s/PostgreSQL stand-alone backend .*/PostgreSQL stand-alone backend ###/
-- end_matchsubs

\!gpstop -M fast -aq;

-- Generate shutdown checkpoint xlog record
\! echo "select 1" | postgres  --single --gp_num_contents_in_cluster=1 -O -c gp_session_role=utility -c gp_debug_linger=0 -c gp_before_filespace_setup=true -D $MASTER_DATA_DIRECTORY template1;

-- We will hit error when the checkpoint xlog record is corrupt
\! echo "select 1" | postgres  --single --gp_num_contents_in_cluster=1 -O -c gp_session_role=utility -c gp_debug_linger=0 -D $MASTER_DATA_DIRECTORY template1;

\!gpstart -aq;
