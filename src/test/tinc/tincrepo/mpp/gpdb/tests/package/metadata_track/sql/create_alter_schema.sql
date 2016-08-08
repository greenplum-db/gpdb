CREATE USER mdt_db_user13;

CREATE SCHEMA mdt_schema1;
CREATE SCHEMA mdt_db_schema5 AUTHORIZATION mdt_user ;
CREATE SCHEMA AUTHORIZATION mdt_user;

ALTER SCHEMA mdt_user RENAME TO mdt_db_schema6;
ALTER SCHEMA  mdt_schema1 OWNER TO mdt_db_user13;

drop schema mdt_schema1;
drop schema mdt_db_schema5;
drop schema mdt_db_schema6;
drop role mdt_db_user13;

