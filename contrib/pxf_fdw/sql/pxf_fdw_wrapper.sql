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

-- Foreign-data wrapper creation fails if wire_format option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( wire_format 'TEXT' );

-- Foreign-data wrapper creation fails if header option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', header 'TRUE' );

-- Foreign-data wrapper creation fails if delimiter option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', delimiter ' ' );

-- Foreign-data wrapper creation fails if quote option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', quote '`' );

-- Foreign-data wrapper creation fails if escape option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', escape '\' );

-- Foreign-data wrapper creation fails if null option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', null '' );

-- Foreign-data wrapper creation fails if encoding option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', encoding 'UTF-8' );

-- Foreign-data wrapper creation fails if newline option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', newline 'CRLF' );

-- Foreign-data wrapper creation fails if fill_missing_fields option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', fill_missing_fields '' );

-- Foreign-data wrapper creation fails if force_null option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', force_null 'true' );

-- Foreign-data wrapper creation fails if force_not_null option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', force_not_null 'true' );

-- Foreign-data wrapper creation fails if reject_limit option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', reject_limit '5' );

-- Foreign-data wrapper creation fails if reject_limit_type option is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', reject_limit_type 'rows' );

-- Foreign-data wrapper succeeds when protocol is provided
CREATE FOREIGN DATA WRAPPER dummy_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'dummy', mpp_execute 'all segments' );

-- Foreign-data wrapper alteration fails when protocol is dropped
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( DROP protocol );

-- Foreign-data wrapper alteration fails if protocol option is empty
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( SET protocol '' );

-- Foreign-data wrapper alteration fails if resource option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD resource '/invalid/option/for/wrapper' );

-- Foreign-data wrapper alteration fails if header option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD header 'TRUE' );

-- Foreign-data wrapper alteration fails if wire_format option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD wire_format 'TEXT' );

-- Foreign-data wrapper alteration fails if delimiter option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD delimiter ' ' );

-- Foreign-data wrapper alteration fails if quote option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD quote '`' );

-- Foreign-data wrapper alteration fails if escape option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD escape '\' );

-- Foreign-data wrapper alteration fails if null option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD null '' );

-- Foreign-data wrapper alteration fails if encoding option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD encoding 'UTF-8' );

-- Foreign-data wrapper alteration fails if newline option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD newline 'CRLF' );

-- Foreign-data wrapper alteration fails if fill_missing_fields option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD fill_missing_fields '' );

-- Foreign-data wrapper alteration fails if force_null option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD force_null 'true' );

-- Foreign-data wrapper alteration fails if force_not_null option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD force_not_null 'true' );

-- Foreign-data wrapper alteration fails if reject_limit option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD reject_limit '5' );

-- Foreign-data wrapper alteration fails if reject_limit_type option is added
ALTER FOREIGN DATA WRAPPER dummy_pxf_fdw
    OPTIONS ( ADD reject_limit_type 'rows' );

