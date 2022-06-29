#
/^-- complain.*CREATE EXTENSION/,/^$/c\
-- Adjust this setting to control where the objects get created.\
SET search_path = public;\
\

#
/^-- Type definitions/a\
\
BEGIN;
#
/^-- type creation is needlessly chatty/a\
\
SET LOCAL client_min_messages = warning;

#
/^COMMENT ON TYPE iprange/a\
\
COMMIT;
#
/^CREATE TYPE [^()]*;/,/^$/d
/^CREATE FUNCTION/s/CREATE FUNCTION/CREATE OR REPLACE FUNCTION/
