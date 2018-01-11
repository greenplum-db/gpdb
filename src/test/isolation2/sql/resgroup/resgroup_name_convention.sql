--
-- Test the resource group name convention.
--
-- Resource group names follow the general object name convention:
-- https://www.postgresql.org/docs/current/static/sql-syntax-lexical.html#SQL-SYNTAX-IDENTIFIERS
--
-- Besides that there are 3 reserved names:
-- * default_group, admin_group: names of the 2 default resource groups;
-- * none: a special name usually used in ALTER ROLE command to reset
--   to the proper default resource group;
--
-- This case is put under isolation2 dir as other resource group cases,
-- although it does not require any extended feature of the isolation2
-- test framework.
--

--
-- setup
--

CREATE OR REPLACE VIEW rg_name_view AS
	SELECT S.rsgname, C.concurrency
	FROM gp_toolkit.gp_resgroup_config C, gp_toolkit.gp_resgroup_status S
	WHERE C.groupid = S.groupid
	  AND C.groupname != 'default_group'
	  AND C.groupname != 'admin_group'
	ORDER BY C.groupid;

-- TODO: need to cleanup all existing resgroups

--
-- positive
--

-- by default resgroup names have the form of [_a-zA-Z][_a-zA-Z0-9]*
CREATE RESOURCE GROUP rgNameTest01 WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP rgNameTest01 SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP rgNameTest01;
CREATE RESOURCE GROUP __rg_name_test_01__ WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP __rg_name_test_01__ SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP __rg_name_test_01__;

-- min length is 1 character
CREATE RESOURCE GROUP Z WITH (cpu_rate_limit=10, memory_limit=10);
DROP   RESOURCE GROUP Z;

-- max length is 63 characters
CREATE RESOURCE GROUP max012345678901234567890123456789012345678901234567890123456789 WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP max012345678901234567890123456789012345678901234567890123456789 SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP max012345678901234567890123456789012345678901234567890123456789;
-- characters exceed the max length are ignored
CREATE RESOURCE GROUP max012345678901234567890123456789012345678901234567890123456789further WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP max012345678901234567890123456789012345678901234567890123456789are SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP max012345678901234567890123456789012345678901234567890123456789ignored;

-- special characters are allowed with double quotation marks
-- white spaces
CREATE RESOURCE GROUP "newlines
s p a c e s
t	a	b	s" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "newlines
s p a c e s
t	a	b	s" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "newlines
s p a c e s
t	a	b	s";
-- punctuations
CREATE RESOURCE GROUP "!#$%&`()*+,-./:;<=>?@[]^_{|}~" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "!#$%&`()*+,-./:;<=>?@[]^_{|}~" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "!#$%&`()*+,-./:;<=>?@[]^_{|}~";
-- quotation marks
CREATE RESOURCE GROUP "'' are 2 single quotation marks" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "'' are 2 single quotation marks" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "'' are 2 single quotation marks";
CREATE RESOURCE GROUP """ is 1 double quotation mark" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP """ is 1 double quotation mark" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP """ is 1 double quotation mark";

-- nothing special with leading character
CREATE RESOURCE GROUP "0 as prefix" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "0 as prefix" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "0 as prefix";
CREATE RESOURCE GROUP " leading space" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP " leading space" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP " leading space";

-- backslash is not used as the escape character
CREATE RESOURCE GROUP "\\ are two backslashes" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "\\ are two backslashes" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "\\ are two backslashes";
-- below are octal, hex and unicode representations of "rg1"
CREATE RESOURCE GROUP "\o162\o147\o61" WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP "\x72\x67\x31" WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP "\u0072\u0067\u0031" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "\o162\o147\o61" SET concurrency 2;
ALTER  RESOURCE GROUP "\x72\x67\x31" SET concurrency 2;
ALTER  RESOURCE GROUP "\u0072\u0067\u0031" SET concurrency 2;
SELECT * FROM rg_name_view;
-- but as \o, \x and \u are not supported,
-- so they are just 3 different names,
-- none of them equals to "rg1".
DROP   RESOURCE GROUP "rg1";
DROP   RESOURCE GROUP "\o162\o147\o61";
DROP   RESOURCE GROUP "\x72\x67\x31";
DROP   RESOURCE GROUP "\u0072\u0067\u0031";

-- unicode escapes are supported
CREATE RESOURCE GROUP U&"\0441\043B\043E\043D" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP U&"\0441\043B\043E\043D" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP U&"\0441\043B\043E\043D";
-- unicode representation of "rg1"
CREATE RESOURCE GROUP U&"\0072\0067\0031" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "rg1" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "rg1";

-- CJK characters are allowed with or without double quotation marks
CREATE RESOURCE GROUP 资源组 WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "资源组" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP 资源组;
CREATE RESOURCE GROUP リソース・グループ WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "リソース・グループ" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP リソース・グループ;
CREATE RESOURCE GROUP 자원그룹 WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "자원그룹" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP 자원그룹;

-- names are case sensitive,
-- but are always converted to lower case unless around with quotation marks
CREATE RESOURCE GROUP "RG_NAME_TEST" WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP rg_Name_Test WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP "rg_name_test" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP Rg_NaMe_TeSt SET concurrency 2;
ALTER  RESOURCE GROUP "RG_NAME_TEST" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "RG_NAME_TEST";
DROP   RESOURCE GROUP RG_nAME_tEST;

-- reserved names are all lower case: "default_group", "admin_group", "none",
-- they can be used by users with at least one upper case character.
CREATE RESOURCE GROUP "None" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "None" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "None";
CREATE RESOURCE GROUP "NONE" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "NONE" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "NONE";
CREATE RESOURCE GROUP "DEFAULT_GROup" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "DEFAULT_GROup" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "DEFAULT_GROup";
CREATE RESOURCE GROUP "ADMIN_GROUP" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "ADMIN_GROUP" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "ADMIN_GROUP";

CREATE RESOURCE GROUP "with" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "with" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "with";
CREATE RESOURCE GROUP "WITH" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "WITH" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "WITH";
CREATE RESOURCE GROUP "group" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "group" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "group";
CREATE RESOURCE GROUP "create" WITH (cpu_rate_limit=10, memory_limit=10);
ALTER  RESOURCE GROUP "create" SET concurrency 2;
SELECT * FROM rg_name_view;
DROP   RESOURCE GROUP "create";

--
-- negative
--

-- does not support single quotation marks around the name
CREATE RESOURCE GROUP 'must_fail' WITH (cpu_rate_limit=10, memory_limit=10);

-- does not support leading numbers
CREATE RESOURCE GROUP 0_must_fail WITH (cpu_rate_limit=10, memory_limit=10);

-- reserved names are not allowed even with double quotation marks
CREATE RESOURCE GROUP "default_group" WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP "admin_group" WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP "none" WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP default_group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP admin_group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP none WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP DEFAULT_GROUP WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP Admin_Group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP NONE WITH (cpu_rate_limit=10, memory_limit=10);

-- keywords are not allowed without quotation marks
CREATE RESOURCE GROUP with WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP WITH WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP group WITH (cpu_rate_limit=10, memory_limit=10);
CREATE RESOURCE GROUP CREATE WITH (cpu_rate_limit=10, memory_limit=10);

-- min length is 1 character
CREATE RESOURCE GROUP "" WITH (cpu_rate_limit=10, memory_limit=10);

