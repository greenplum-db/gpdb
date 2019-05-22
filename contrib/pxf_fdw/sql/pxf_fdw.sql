-- ===================================================================
-- create FDW objects
-- ===================================================================

CREATE EXTENSION pxf_fdw;

DROP ROLE IF EXISTS pxf_fdw_user;
CREATE ROLE pxf_fdw_user;

-- ===================================================================
-- Validation for WRAPPER options
-- ===================================================================

-- When 'protocol' option is not provided during WRAPPER creation, an error
-- should be given to the user and the WRAPPER creation should be aborted.
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator;

-- When 'protocol' option is blank during WRAPPER creation, an error
-- should be given to the user and the WRAPPER creation should be aborted.
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol '' );

-- Foreign Data Wrapper Succeeds when protocol is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy' );

-- When 'protocol' option is dropped, an error should be given to the user
-- and the WRAPPER alteration should be aborted.
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( DROP protocol );

-- ===================================================================
-- Validation for SERVER options
-- ===================================================================

-- Server creation fails if protocol option is provided
CREATE SERVER dummy_server
    FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( protocol 'dummy2' );

-- Server creation succeeds if protocol option is not provided
CREATE SERVER dummy_server
    FOREIGN DATA WRAPPER dummy_pxf_fdw;

-- Altering a server fails if protocol option is added
ALTER SERVER dummy_server
    OPTIONS ( ADD protocol 'dummy2' );

-- ===================================================================
-- Validation for USER MAPPING options
-- ===================================================================

-- User mapping creation fails if protocol option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( protocol 'usermappingprotocol' );

-- User mapping creation succeeds if protocol option is not provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server;

-- User mapping alteration fails if protocol option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD protocol 'usermappingprotocol' );

-- ===================================================================
-- Validation for TABLE options
-- ===================================================================

-- Table creation fails if protocol option is provided
CREATE FOREIGN TABLE dummy_table (id int, name text)
    SERVER dummy_server
    OPTIONS ( protocol 'dummy2' );

-- Table creation fails if resource is not provided
CREATE FOREIGN TABLE dummy_table (id int, name text)
    SERVER dummy_server;

-- Table creation succeeds if resource is provided and protocol is not provided
CREATE FOREIGN TABLE dummy_table (id int, name text)
    SERVER dummy_server
    OPTIONS ( resource '/path/to/resource' );

-- Table alteration fails if protocol option is added
ALTER FOREIGN TABLE dummy_table
    OPTIONS ( ADD protocol 'table_protocol' );

-- Table alteration fails if resource option is dropped
ALTER FOREIGN TABLE dummy_table
    OPTIONS ( DROP resource );

-- Table alteration succeeds if resource option is set
ALTER FOREIGN TABLE dummy_table
    OPTIONS ( SET resource '/new/path/to/resource' );
