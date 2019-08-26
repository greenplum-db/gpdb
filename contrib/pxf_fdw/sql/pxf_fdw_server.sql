-- ===================================================================
-- Validation for SERVER options
-- ===================================================================

--
-- Server creation fails if protocol option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( protocol 'pxf_fdw_test2' );

--
-- Server creation fails if resource option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( resource '/invalid/option/for/server' );

--
-- Server creation fails if delimiter option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( delimiter ' ' );

--
-- Server creation fails if header option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( header 'TRUE' );

--
-- Server creation fails if quote option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( quote '`' );

--
-- Server creation fails if escape option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( escape '\' );

--
-- Server creation fails if null option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( null '' );

--
-- Server creation fails if encoding option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( encoding 'UTF-8' );

--
-- Server creation fails if newline option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( newline 'CRLF' );

--
-- Server creation fails if fill_missing_fields option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( fill_missing_fields '' );

--
-- Server creation fails if force_null option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( force_null 'true' );

--
-- Server creation fails if force_not_null option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( force_not_null 'true' );

--
-- Server creation fails if reject_limit option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( reject_limit '5' );

--
-- Server creation fails if reject_limit_type option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( reject_limit_type 'rows' );

--
-- Server creation fails if log_errors option is provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( log_errors 'true' );

--
-- Server creation succeeds if protocol option is not provided
--
CREATE SERVER pxf_fdw_test_server
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw;

--
-- Server creation succeeds if config option is provided
--
CREATE SERVER pxf_fdw_test_server_with_config
    FOREIGN DATA WRAPPER pxf_fdw_test_pxf_fdw
    OPTIONS ( config '/foo/bar' );

--
-- Server alteration fails if protocol option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD protocol 'pxf_fdw_test2' );

--
-- Server alteration fails if resource option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD resource '/invalid/option/for/server' );

--
-- Server alteration fails if header option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD header 'TRUE' );

--
-- Server alteration fails if delimiter option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD delimiter ' ' );

--
-- Server alteration fails if quote option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD quote '`' );

--
-- Server alteration fails if escape option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD escape '\' );

--
-- Server alteration fails if null option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD null '' );

--
-- Server alteration fails if encoding option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD encoding 'UTF-8' );

--
-- Server alteration fails if newline option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD newline 'CRLF' );

--
-- Server alteration fails if fill_missing_fields option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD fill_missing_fields '' );

--
-- Server alteration fails if force_null option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD force_null 'true' );

--
-- Server alteration fails if force_not_null option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD force_not_null 'true' );

--
-- Server alteration fails if reject_limit option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD reject_limit '5' );

--
-- Server alteration fails if reject_limit_type option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD reject_limit_type 'rows' );

--
-- Server alteration fails if log_errors option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD log_errors 'true' );

--
-- Server alteration succeeds if config option is added
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( ADD config '/foo/bar' );

--
-- Server alteration succeeds if config option is modified
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( SET config '/foo/bar' );

--
-- Server alteration succeeds if config option is dropped
--
ALTER SERVER pxf_fdw_test_server
    OPTIONS ( DROP config );

