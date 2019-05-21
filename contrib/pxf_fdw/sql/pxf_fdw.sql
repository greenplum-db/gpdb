-- ===================================================================
-- create FDW objects
-- ===================================================================

CREATE EXTENSION pxf_fdw;

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