-- PREPARE TRANSACTION should not work
BEGIN;
PREPARE TRANSACTION 'foo_prep_xact';

-- PREPARE TRANSACTION should not work in utility-mode connections either
\! PGOPTIONS='-c gp_role=utility' psql -X regression -c "BEGIN; PREPARE TRANSACTION 'foo_prep_xact';"
