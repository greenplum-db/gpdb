-- start_matchignore
-- m/.*-/
-- end_matchignore
\! echo 'EXPANSION_PREPARE_STARTED:<path> to inputfile' > $MASTER_DATA_DIRECTORY/gpexpand.status
\! gpcheckcat
\! gpconfig -r gp_debug_linger
\! gppkg
\! rm $MASTER_DATA_DIRECTORY/gpexpand.status
