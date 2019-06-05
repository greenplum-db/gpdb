-- ===================================================================
-- create FDW objects
-- ===================================================================

CREATE EXTENSION pxf_fdw;

DROP ROLE IF EXISTS pxf_fdw_user;
CREATE ROLE pxf_fdw_user;

-- ===================================================================
-- Validation for WRAPPER options
-- ===================================================================

-- Foreign-data wrapper creation fails if protocol option is not provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator;

-- Foreign-data wrapper creation fails if protocol option is empty
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol '' );

-- Foreign-data wrapper creation fails if resource option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( resource '/invalid/option/for/wrapper' );

-- Foreign-data wrapper succeeds when protocol is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy' );

-- Foreign-data wrapper alteration fails when protocol is dropped
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( DROP protocol );

-- Foreign-data wrapper alteration fails if protocol option is empty
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( SET protocol '' );

-- Foreign-data wrapper alteration fails if resource option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD resource '/invalid/option/for/wrapper' );

-- ===================================================================
-- Validation for SERVER options
-- ===================================================================

-- Server creation fails if protocol option is provided
CREATE SERVER dummy_server
    FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( protocol 'dummy2' );

-- Server creation fails if resource option is provided
CREATE SERVER dummy_server
    FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( resource '/invalid/option/for/server' );

-- Server creation succeeds if protocol option is not provided
CREATE SERVER dummy_server
    FOREIGN DATA WRAPPER dummy_pxf_fdw;

-- Server alteration fails if protocol option is added
ALTER SERVER dummy_server
    OPTIONS ( ADD protocol 'dummy2' );

-- Server alteration fails if resource option is added
ALTER SERVER dummy_server
    OPTIONS ( ADD resource '/invalid/option/for/server' );

-- ===================================================================
-- Validation for USER MAPPING options
-- ===================================================================

-- User mapping creation fails if protocol option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( protocol 'usermappingprotocol' );

-- User mapping creation fails if resource option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( resource '/invalid/option/for/user/mapping' );

-- User mapping creation succeeds if protocol option is not provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server;

-- User mapping alteration fails if protocol option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD protocol 'usermappingprotocol' );

-- User mapping alteration fails if resource option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD resource '/invalid/option/for/user/mapping' );

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

-- Table creation fails if resource is provided as an empty string
CREATE FOREIGN TABLE dummy_table (id int, name text)
    SERVER dummy_server
    OPTIONS ( resource '' );

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

-- Table alteration fails if resource is provided as an empty string
ALTER FOREIGN TABLE dummy_table
    OPTIONS ( SET resource '' );

-- Table alteration succeeds if resource option is set
ALTER FOREIGN TABLE dummy_table
    OPTIONS ( SET resource '/new/path/to/resource' );
