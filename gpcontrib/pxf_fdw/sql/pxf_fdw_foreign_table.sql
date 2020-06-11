-- start_matchsubs
-- m/ERROR:  COPY quote available only in CSV mode/
-- s/ERROR:  COPY quote available only in CSV mode/ERROR:  quote available only in CSV mode/
-- end_matchsubs
-- ===================================================================
-- Validation for TABLE options
-- ===================================================================

--
-- Table creation fails if protocol option is provided
--
CREATE FOREIGN TABLE pxf_fdw_test_table (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( protocol 'pxf_fdw_test2' );

--
-- Table creation fails if resource is not provided
--
CREATE FOREIGN TABLE pxf_fdw_test_table (id int, name text)
    SERVER pxf_fdw_test_server;

--
-- Table creation fails if resource is provided as an empty string
--
CREATE FOREIGN TABLE pxf_fdw_test_table (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '' );

--
-- Table creation fails if config option is provided
--
CREATE FOREIGN TABLE pxf_fdw_test_table ()
    SERVER pxf_fdw_test_server
    OPTIONS ( config '/foo/bar' );

--
-- Table creation succeeds if delimiter is provided with a single one-byte char
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_delim (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', delimiter '|' );

--
-- Table creation succeeds if delimiter is provided with a single one-byte char
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_quote (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', format 'csv', quote '`' );

--
-- Table creation succeeds if escape is provided with a single one-byte char
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_escape (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', format 'csv', escape '\' );

--
-- Table creation succeeds if null is provided with a single one-byte char
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_null (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', null '' );

--
-- Table creation succeeds if encoding is provided with a single one-byte char
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_encoding (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', encoding 'UTF-8' );

--
-- Table creation succeeds if newline is valid
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_newline (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', newline 'CRLF' );

--
-- Table creation succeeds if resource and fill_missing_fields are provided
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_fill_missing_fields (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', fill_missing_fields 'true');

--
-- Table creation succeeds if force_null is provided on column definition
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_force_null (
      id int OPTIONS (force_null 'true'),
      name text OPTIONS (force_null 'false')
    ) SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource' );

--
-- Table creation succeeds if force_not_null is provided on column definition
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_force_not_null (
      id int OPTIONS (force_not_null 'true'),
      name text OPTIONS (force_not_null 'false')
    ) SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource' );

--
-- Table creation succeeds if resource is provided and header is valid
--
CREATE FOREIGN TABLE pxf_fdw_test_table_with_header (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', header 'TRUE' );

--
-- Table creation succeeds if resource is provided and protocol is not provided
--
CREATE FOREIGN TABLE pxf_fdw_test_table (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', extra 'extra_value' );

--
-- Table creation fails if reject_limit is < 1
--
CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( reject_limit '-4' );

CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( reject_limit '0' );

--
-- Table creation fails if reject_limit_type is not 'percent' or 'rows'
--
CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit_type (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( reject_limit_type 'not-percent-or-rows' );

--
-- Table creation fails if reject_limit is non-numeric
--
CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit 'no-number-here' );

--
-- Table creation fails if reject_limit_type is 'rows' and reject_limit < 2
--
CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit_type (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit '1', reject_limit_type 'rows' );

--
-- Table creation fails if reject_limit_type is 'percent' and not between 1-100
--
CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit_type (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit '-1', reject_limit_type 'percent' );

CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit_type (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit '101', reject_limit_type 'percent' );

--
-- Table creation fails if only resource and log errors are set
--
CREATE FOREIGN TABLE pxf_fdw_test_table_log_errors (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', log_errors 'true' );

--
-- Table creation fails if only log_errors is non-boolean
--
CREATE FOREIGN TABLE pxf_fdw_test_table_log_errors (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit '4', log_errors 'yes' );

--
-- Table creation fails if quote is provided (CSV-only option)
--
CREATE FOREIGN TABLE pxf_fdw_test_table_csv_only ()
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/foo', quote '9' );

--
-- Table creation succeeds if resource is provided and reject_limit is provided correctly
--
CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit '4' );

--
-- Table creation succeeds if resource is provided and reject_limit_type is provided correctly
--
CREATE FOREIGN TABLE pxf_fdw_test_table_reject_limit_type (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit_type 'rows', reject_limit '4' );

--
-- Table create succeeds if resource, reject_limit, and log_errors are provided correctly
--
CREATE FOREIGN TABLE pxf_fdw_test_table_log_errors (id int, name text)
    SERVER pxf_fdw_test_server
    OPTIONS ( resource '/path/to/resource', reject_limit '4', log_errors 'true' );

--
-- Table alteration fails if protocol option is added
--
ALTER FOREIGN TABLE pxf_fdw_test_table
    OPTIONS ( ADD protocol 'table_protocol' );

--
-- Table alteration fails if resource option is dropped
--
ALTER FOREIGN TABLE pxf_fdw_test_table
    OPTIONS ( DROP resource );

--
-- Table alteration fails if resource is provided as an empty string
--
ALTER FOREIGN TABLE pxf_fdw_test_table
    OPTIONS ( SET resource '' );

--
-- Table alteration succeeds if resource option is set
--
ALTER FOREIGN TABLE pxf_fdw_test_table
    OPTIONS ( SET resource '/new/path/to/resource' );

--
-- Table alteration succeeds if header is provided correctly
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_header
    OPTIONS ( SET header 'FALSE' );

--
-- Table alteration succeeds if delimiter is a single one-byte char
--
ALTER FOREIGN TABLE pxf_fdw_test_table
    OPTIONS ( ADD delimiter ',' );

--
-- Table alteration succeeds if quote is a single one-byte char
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_quote
    OPTIONS ( SET quote '"' );

--
-- Table alteration succeeds if escape is a single one-byte char
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_escape
    OPTIONS ( SET escape '+' );

--
-- Table alteration succeeds if null is a single one-byte char
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_null
    OPTIONS ( SET null 'null' );

--
-- Table alteration succeeds if encoding is a single one-byte char
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_encoding
    OPTIONS ( SET encoding 'ISO-8859-1' );

--
-- Table alteration succeeds if newline is valid
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_newline
    OPTIONS ( SET newline 'CRLF' );

--
-- Table alteration succeeds when fill_missing_fields is provided
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_fill_missing_fields
    OPTIONS ( SET fill_missing_fields 'true' );

--
-- Table alteration succeeds when force_null is provided on column definition
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_force_null
    ALTER COLUMN id OPTIONS (DROP force_null);

--
-- Table alteration succeeds when force_not_null is provided on column definition
--
ALTER FOREIGN TABLE pxf_fdw_test_table_with_force_not_null
    ALTER COLUMN id OPTIONS (DROP force_not_null);

--
-- Table alteration fails if reject_limit is less than 1
--
ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit
    OPTIONS ( SET reject_limit '-4' );

ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit
    OPTIONS ( SET reject_limit '0' );

--
-- Table alteration fails if reject_limit_type is not 'percent' or 'rows'
--
ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit_type
    OPTIONS ( SET reject_limit_type 'not-percent-or-rows' );

--
-- Table alteration fails if reject_limit is non-numeric
--
ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit
    OPTIONS ( SET reject_limit 'no-number-here' );

--
-- Table alteration fails if reject_limit_type is rows and reject_limit is < 2
--
ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit_type
    OPTIONS ( SET reject_limit '1', SET reject_limit_type 'rows' );

--
-- Table alteration fails if reject_limit_type is percent and reject_limit is not within 1-100
--
ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit_type
    OPTIONS ( SET reject_limit '-1', SET reject_limit_type 'percent' );

ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit_type
    OPTIONS ( SET reject_limit '101', SET reject_limit_type 'percent' );

--
-- Table alteration succeeds if resource is provided and reject_limit is provided correctly
--
ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit
    OPTIONS ( SET reject_limit '5' );

--
-- Table alteration succeeds if resource is provided and reject_limit_type is provided correctly
--
ALTER FOREIGN TABLE pxf_fdw_test_table_reject_limit_type
    OPTIONS ( SET reject_limit_type 'percent' );

--
-- Table alteration succeeds for log_errors if table has valid resource and reject_limit
--
ALTER FOREIGN TABLE pxf_fdw_test_table_log_errors
    OPTIONS ( SET log_errors 'false' );

--
-- Table alteration fails for log_errors if table has valid resource but no reject_limit
--
ALTER FOREIGN TABLE pxf_fdw_test_table
    OPTIONS ( ADD log_errors 'false' );

--
-- Table alteration fails for log_errors if log_errors is non-boolean
--
ALTER FOREIGN TABLE pxf_fdw_test_table_log_errors
    OPTIONS ( SET log_errors 'no' );

--
-- Table alteration fails if config option is provided
--
ALTER FOREIGN TABLE pxf_fdw_test_table
    OPTIONS ( ADD config '/foo/bar' );
