--
-- Test foreign-data wrapper gp_exttable_fdw.
--

CREATE EXTENSION IF NOT EXISTS gp_exttable_fdw;

CREATE SERVER IF NOT EXISTS gp_exttable_server FOREIGN DATA WRAPPER gp_exttable_fdw;

-- validator tests
CREATE FOREIGN TABLE ext_without_execute_on_option (id int, category text) SERVER gp_exttable_server;  -- ERROR
