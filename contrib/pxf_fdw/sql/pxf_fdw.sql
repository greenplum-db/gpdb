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

-- When 'protocol' option is dropped, an error should be given to the user
-- and the WRAPPER alteration should be aborted.
ALTER FOREIGN DATA WRAPPER jdbc_pxf_fdw
    OPTIONS ( DROP protocol );