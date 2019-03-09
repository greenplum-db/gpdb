-- start_matchsubs
-- m/^\S+ (.*):.*:.*-(\[\S+\]):-/
-- s/^\S+ (.*):.*:.*-(\[\S+\]):-/TIMESTAMP $1:HOST:USER-$2:-/
-- end_matchsubs

\! echo 'EXPANSION_PREPARE_STARTED:<path> to inputfile' > $MASTER_DATA_DIRECTORY/gpexpand.status
\! gpcheckcat
\! gpconfig -r gp_debug_linger
-- most gppkg actions should be disallowed while gpexpand is in progress
\! gppkg --query no-such-package
-- the only exception is 'build' which has no interaction with the cluster
\! gppkg --build no-such-package
\! rm $MASTER_DATA_DIRECTORY/gpexpand.status
