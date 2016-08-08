
-- MPP-8466: set this GUC so that we can create database with latin8 encoding
-- 20100412: Ngoc
-- 20100414: Ngoc: GUC is not ported to 4.0 yet => remove encoding='latin8' - approved by Anu
-- SET gp_check_locale_encoding_compatibility = off;

CREATE ROLE mtd_db_owner1;
CREATE ROLE mtd_db_owner2;

--CREATE DATABASE mtd_db_name1 WITH OWNER = mtd_db_owner1 TEMPLATE =template1 ENCODING='latin8'  CONNECTION LIMIT= 2;
CREATE DATABASE mtd_db_name1 WITH OWNER = mtd_db_owner1 TEMPLATE =template1 CONNECTION LIMIT= 2;
ALTER DATABASE mtd_db_name1 WITH  CONNECTION LIMIT 3;

CREATE DATABASE mtd_db_name2 with OWNER=mtd_db_owner1;
ALTER DATABASE mtd_db_name2  RENAME TO mtd_new_db_name2;

CREATE DATABASE mtd_new_db_name1 with OWNER=mtd_db_owner1;
ALTER DATABASE mtd_new_db_name1  OWNER TO mtd_db_owner2;

CREATE SCHEMA mtd_myschema;
CREATE DATABASE mtd_db_name3;
ALTER DATABASE mtd_db_name3 SET search_path TO mtd_myschema, public, pg_catalog;

CREATE DATABASE mtd_db_name4;
ALTER DATABASE mtd_db_name4 SET search_path TO mtd_myschema, public, pg_catalog;
ALTER DATABASE mtd_db_name4 RESET search_path;

CREATE DATABASE mtd_db_name5;
ALTER DATABASE mtd_db_name5 SET search_path TO mtd_myschema, public, pg_catalog;
ALTER DATABASE mtd_db_name5 RESET ALL;

drop database mtd_db_name1;
drop database mtd_db_name3;
drop database mtd_db_name4;
drop database mtd_db_name5;
drop database mtd_new_db_name1;
drop database mtd_new_db_name2;
drop schema mtd_myschema;
drop role mtd_db_owner1;
drop role mtd_db_owner2;

