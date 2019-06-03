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

-- User mapping creation fails if wire_format option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( wire_format 'TEXT' );

-- User mapping creation fails if header option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( header 'TRUE' );

-- User mapping creation fails if delimiter option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( delimiter ' ' );

-- User mapping creation fails if quote option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( quote '`' );

-- User mapping creation fails if escape option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( escape '\' );

-- User mapping creation fails if null option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( null '' );

-- User mapping creation fails if encoding option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( encoding 'UTF-8' );

-- User mapping creation fails if newline option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( newline 'CRLF' );

-- User mapping creation fails if fill_missing_fields option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( fill_missing_fields '' );

-- User mapping creation fails if force_null option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( force_null 'true' );

-- User mapping creation fails if force_not_null option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( force_not_null 'true' );

-- User mapping creation fails if reject_limit option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( reject_limit '5' );

-- User mapping creation fails if reject_limit_type option is provided
CREATE USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( reject_limit_type 'rows' );

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

-- User mapping alteration fails if header option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD header 'TRUE' );

-- User mapping alteration fails if wire_format option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD wire_format 'TEXT' );

-- User mapping alteration fails if delimiter option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD delimiter ' ' );

-- User mapping alteration fails if quote option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD quote '`' );

-- User mapping alteration fails if escape option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD escape '\' );

-- User mapping alteration fails if null option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD null '' );

-- User mapping alteration fails if encoding option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD encoding 'UTF-8' );

-- User mapping alteration fails if newline option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD newline 'CRLF' );

-- User mapping alteration fails if fill_missing_fields option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD fill_missing_fields '' );

-- User mapping alteration fails if force_null option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD force_null 'true' );

-- User mapping alteration fails if force_not_null option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD force_not_null 'true' );

-- User mapping alteration fails if reject_limit option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD reject_limit '5' );

-- User mapping alteration fails if reject_limit_type option is added
ALTER USER MAPPING FOR pxf_fdw_user
    SERVER dummy_server
    OPTIONS ( ADD reject_limit_type 'rows' );

