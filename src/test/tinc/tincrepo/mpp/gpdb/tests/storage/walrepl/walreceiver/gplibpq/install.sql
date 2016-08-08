-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE OR REPLACE FUNCTION test_connect(text) RETURNS bool AS
'$libdir/gplibpq' LANGUAGE C;

CREATE OR REPLACE FUNCTION test_disconnect() RETURNS bool AS
'$libdir/gplibpq' LANGUAGE C;

CREATE OR REPLACE FUNCTION test_receive() RETURNS bool AS
'$libdir/gplibpq' LANGUAGE C;

CREATE OR REPLACE FUNCTION test_send() RETURNS bool AS
'$libdir/gplibpq' LANGUAGE C;

CREATE OR REPLACE FUNCTION test_scenario1(text) RETURNS bool AS
'$libdir/gplibpq' LANGUAGE C;
