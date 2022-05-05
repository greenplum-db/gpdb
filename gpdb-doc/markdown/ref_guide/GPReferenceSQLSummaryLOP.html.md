---
title: SQL Syntax Summary 
---

## <a id="ae1903297"></a>ABORT 

Terminates the current transaction.

```
ABORT [WORK | TRANSACTION]
```

See [ABORT](sql_commands/ABORT.html) for more information.

## <a id="ae1903303"></a>ALTER AGGREGATE 

Changes the definition of an aggregate function

```
ALTER AGGREGATE <name> ( <aggregate_signature> )  RENAME TO <new_name>

ALTER AGGREGATE <name> ( <aggregate_signature> ) OWNER TO <new_owner>

ALTER AGGREGATE <name> ( <aggregate_signature> ) SET SCHEMA <new_schema>
```

See [ALTER AGGREGATE](sql_commands/ALTER_AGGREGATE.html) for more information.

## <a id="fixme"></a>ALTER COLLATION 

Changes the definition of a collation.

```
ALTER COLLATION <name> RENAME TO <new_name>

ALTER COLLATION <name> OWNER TO <new_owner>

ALTER COLLATION <name> SET SCHEMA <new_schema>
```

See [ALTER COLLATION](sql_commands/ALTER_COLLATION.html) for more information.

## <a id="ae1903313"></a>ALTER CONVERSION 

Changes the definition of a conversion.

```
ALTER CONVERSION <name> RENAME TO <newname>

ALTER CONVERSION <name> OWNER TO <newowner>

ALTER CONVERSION <name> SET SCHEMA <new_schema>

```

See [ALTER CONVERSION](sql_commands/ALTER_CONVERSION.html) for more information.

## <a id="ae1903321"></a>ALTER DATABASE 

Changes the attributes of a database.

```
ALTER DATABASE <name> [ WITH CONNECTION LIMIT <connlimit> ]

ALTER DATABASE <name> RENAME TO <newname>

ALTER DATABASE <name> OWNER TO <new_owner>

ALTER DATABASE <name> SET TABLESPACE <new_tablespace>

ALTER DATABASE <name> SET <parameter> { TO | = } { <value> | DEFAULT }
ALTER DATABASE <name> SET <parameter> FROM CURRENT
ALTER DATABASE <name> RESET <parameter>
ALTER DATABASE <name> RESET ALL

```

See [ALTER DATABASE](sql_commands/ALTER_DATABASE.html) for more information.

## <a id="fixme"></a>ALTER DEFAULT PRIVILEGES 

Changes default access privileges.

```

ALTER DEFAULT PRIVILEGES
    [ FOR { ROLE | USER } <target_role> [, ...] ]
    [ IN SCHEMA <schema_name> [, ...] ]
    <abbreviated_grant_or_revoke>

where <abbreviated_grant_or_revoke> is one of:

GRANT { { SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER }
    [, ...] | ALL [ PRIVILEGES ] }
    ON TABLES
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { { USAGE | SELECT | UPDATE }
    [, ...] | ALL [ PRIVILEGES ] }
    ON SEQUENCES
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { EXECUTE | ALL [ PRIVILEGES ] }
    ON FUNCTIONS
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON TYPES
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

REVOKE [ GRANT OPTION FOR ]
    { { SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER }
    [, ...] | ALL [ PRIVILEGES ] }
    ON TABLES
    FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
    [ CASCADE | RESTRICT ]

REVOKE [ GRANT OPTION FOR ]
    { { USAGE | SELECT | UPDATE }
    [, ...] | ALL [ PRIVILEGES ] }
    ON SEQUENCES
    FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
    [ CASCADE | RESTRICT ]

REVOKE [ GRANT OPTION FOR ]
    { EXECUTE | ALL [ PRIVILEGES ] }
    ON FUNCTIONS
    FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
    [ CASCADE | RESTRICT ]

REVOKE [ GRANT OPTION FOR ]
    { USAGE | ALL [ PRIVILEGES ] }
    ON TYPES
    FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
    [ CASCADE | RESTRICT ]

```

See [ALTER DEFAULT PRIVILEGES](sql_commands/ALTER_DEFAULT_PRIVILEGES.html) for more information.

## <a id="ae1903335"></a>ALTER DOMAIN 

Changes the definition of a domain.

```
ALTER DOMAIN <name> { SET DEFAULT <expression> | DROP DEFAULT }

ALTER DOMAIN <name> { SET | DROP } NOT NULL

ALTER DOMAIN <name> ADD <domain_constraint> [ NOT VALID ]

ALTER DOMAIN <name> DROP CONSTRAINT [ IF EXISTS ] <constraint_name> [RESTRICT | CASCADE]

ALTER DOMAIN <name> RENAME CONSTRAINT <constraint_name> TO <new_constraint_name>

ALTER DOMAIN <name> VALIDATE CONSTRAINT <constraint_name>
  
ALTER DOMAIN <name> OWNER TO <new_owner>
  
ALTER DOMAIN <name> RENAME TO <new_name>

ALTER DOMAIN <name> SET SCHEMA <new_schema>
```

See [ALTER DOMAIN](sql_commands/ALTER_DOMAIN.html) for more information.

## <a id="fixme"></a>ALTER EXTENSION 

Change the definition of an extension that is registered in a Greenplum database.

```
ALTER EXTENSION <name> UPDATE [ TO <new_version> ]
ALTER EXTENSION <name> SET SCHEMA <new_schema>
ALTER EXTENSION <name> ADD <member_object>
ALTER EXTENSION <name> DROP <member_object>

where <member_object> is:

  ACCESS METHOD <object_name> |
  AGGREGATE <aggregate_name> ( <aggregate_signature> ) |
  CAST (<source_type> AS <target_type>) |
  COLLATION <object_name> |
  CONVERSION <object_name> |
  DOMAIN <object_name> |
  EVENT TRIGGER <object_name> |
  FOREIGN DATA WRAPPER <object_name> |
  FOREIGN TABLE <object_name> |
  FUNCTION <function_name> ( [ [ <argmode> ] [ <argname> ] <argtype> [, ...] ] ) |
  MATERIALIZED VIEW <object_name> |
  OPERATOR <operator_name> (<left_type>, <right_type>) |
  OPERATOR CLASS <object_name> USING <index_method> |
  OPERATOR FAMILY <object_name> USING <index_method> |
  [ PROCEDURAL ] LANGUAGE <object_name> |
  SCHEMA <object_name> |
  SEQUENCE <object_name> |
  SERVER <object_name> |
  TABLE <object_name> |
  TEXT SEARCH CONFIGURATION <object_name> |
  TEXT SEARCH DICTIONARY <object_name> |
  TEXT SEARCH PARSER <object_name> |
  TEXT SEARCH TEMPLATE <object_name> |
  TRANSFORM FOR <type_name> LANGUAGE <lang_name> |
  TYPE <object_name> |
  VIEW <object_name>

and <aggregate_signature> is:

* |
[ <argmode> ] [ <argname> ] <argtype> [ , ... ] |
[ [ <argmode> ] [ <argname> ] <argtype> [ , ... ] ] ORDER BY [ <argmode> ] [ <argname> ] <argtype> [ , ... ]
```

See [ALTER EXTENSION](sql_commands/ALTER_EXTENSION.html) for more information.

## <a id="ae1903351"></a>ALTER EXTERNAL TABLE 

Changes the definition of an external table.

``` {#d84e24}
ALTER EXTERNAL TABLE <name> <action> [, ... ]
```

where action is one of:

```
  ADD [COLUMN] <new_column> <type>
  DROP [COLUMN] <column> [RESTRICT|CASCADE]
  ALTER [COLUMN] <column> TYPE <type>
  OWNER TO <new_owner>
```

See [ALTER EXTERNAL TABLE](sql_commands/ALTER_EXTERNAL_TABLE.html) for more information.

## <a id="ae1903381w"></a>ALTER FOREIGN DATA WRAPPER 

Changes the definition of a foreign-data wrapper.

```
ALTER FOREIGN DATA WRAPPER <name>
    [ HANDLER <handler_function> | NO HANDLER ]
    [ VALIDATOR <validator_function> | NO VALIDATOR ]
    [ OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ] ) ]

ALTER FOREIGN DATA WRAPPER <name> OWNER TO <new_owner>
ALTER FOREIGN DATA WRAPPER <name> RENAME TO <new_name>
```

See [ALTER FOREIGN DATA WRAPPER](sql_commands/ALTER_FOREIGN_DATA_WRAPPER.html) for more information.

## <a id="ae1903381t"></a>ALTER FOREIGN TABLE 

Changes the definition of a foreign table.

```
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    <action> [, ... ]
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    RENAME [ COLUMN ] <column_name> TO <new_column_name>
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    RENAME TO <new_name>
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    SET SCHEMA <new_schema>

```

See [ALTER FOREIGN TABLE](sql_commands/ALTER_FOREIGN_TABLE.html) for more information.

## <a id="ae1903381"></a>ALTER FUNCTION 

Changes the definition of a function.

```
ALTER FUNCTION <name> ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) 
   <action> [, ... ] [RESTRICT]

ALTER FUNCTION <name> ( [ [<argmode>] [<argname>] <argtype> [, ...] ] )
   RENAME TO <new_name>

ALTER FUNCTION <name> ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) 
   OWNER TO <new_owner>

ALTER FUNCTION <name> ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) 
   SET SCHEMA <new_schema>
```

See [ALTER FUNCTION](sql_commands/ALTER_FUNCTION.html) for more information.

## <a id="ae1903401"></a>ALTER GROUP 

Changes a role name or membership.

```
ALTER GROUP <groupname> ADD USER <username> [, ... ]

ALTER GROUP <groupname> DROP USER <username> [, ... ]

ALTER GROUP <groupname> RENAME TO <newname>
```

See [ALTER GROUP](sql_commands/ALTER_GROUP.html) for more information.

## <a id="ae1903411"></a>ALTER INDEX 

Changes the definition of an index.

```
ALTER INDEX [ IF EXISTS ] <name> RENAME TO <new_name>

ALTER INDEX [ IF EXISTS ] <name> SET TABLESPACE <tablespace_name>

ALTER INDEX [ IF EXISTS ] <name> SET ( <storage_parameter> = <value> [, ...] )

ALTER INDEX [ IF EXISTS ] <name> RESET ( <storage_parameter>  [, ...] )

ALTER INDEX ALL IN TABLESPACE <name> [ OWNED BY <role_name> [, ... ] ]
  SET TABLESPACE <new_tablespace> [ NOWAIT ]

```

See [ALTER INDEX](sql_commands/ALTER_INDEX.html) for more information.

## <a id="ae1903423"></a>ALTER LANGUAGE 

Changes the name of a procedural language.

```
ALTER LANGUAGE <name> RENAME TO <newname>
ALTER LANGUAGE <name> OWNER TO <new_owner>
```

See [ALTER LANGUAGE](sql_commands/ALTER_LANGUAGE.html) for more information.

## <a id="fixme"></a>ALTER MATERIALIZED VIEW 

Changes the definition of a materialized view.

```
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name> <action> [, ... ]
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name>
    RENAME [ COLUMN ] <column_name> TO <new_column_name>
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name>
    RENAME TO <new_name>
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name>
    SET SCHEMA <new_schema>
ALTER MATERIALIZED VIEW ALL IN TABLESPACE <name> [ OWNED BY <role_name> [, ... ] ]
    SET TABLESPACE <new_tablespace> [ NOWAIT ]

where <action> is one of:

    ALTER [ COLUMN ] <column_name> SET STATISTICS <integer>
    ALTER [ COLUMN ] <column_name> SET ( <attribute_option> = <value> [, ... ] )
    ALTER [ COLUMN ] <column_name> RESET ( <attribute_option> [, ... ] )
    ALTER [ COLUMN ] <column_name> SET STORAGE { PLAIN | EXTERNAL | EXTENDED | MAIN }
    CLUSTER ON <index_name>
    SET WITHOUT CLUSTER
    SET ( <storage_paramete>r = <value> [, ... ] )
    RESET ( <storage_parameter> [, ... ] )
    OWNER TO <new_owner>
```

See [ALTER MATERIALIZED VIEW](sql_commands/ALTER_MATERIALIZED_VIEW.html) for more information.

## <a id="ae1903429"></a>ALTER OPERATOR 

Changes the definition of an operator.

```
ALTER OPERATOR <name> ( {<left_type> | NONE} , {<right_type> | NONE} ) 
   OWNER TO <new_owner>

ALTER OPERATOR <name> ( {<left_type> | NONE} , {<right_type> | NONE} ) 
    SET SCHEMA <new_schema>

```

See [ALTER OPERATOR](sql_commands/ALTER_OPERATOR.html) for more information.

## <a id="ae1903435"></a>ALTER OPERATOR CLASS 

Changes the definition of an operator class.

```
ALTER OPERATOR CLASS <name> USING <index_method> RENAME TO <new_name>

ALTER OPERATOR CLASS <name> USING <index_method> OWNER TO <new_owner>

ALTER OPERATOR CLASS <name> USING <index_method> SET SCHEMA <new_schema>
```

See [ALTER OPERATOR CLASS](sql_commands/ALTER_OPERATOR_CLASS.html) for more information.

## <a id="fixme"></a>ALTER OPERATOR FAMILY 

Changes the definition of an operator family.

```
ALTER OPERATOR FAMILY <name> USING <index_method> ADD
  {  OPERATOR <strategy_number> <operator_name> ( <op_type>, <op_type> ) [ FOR SEARCH | FOR ORDER BY <sort_family_name> ]
    | FUNCTION <support_number> [ ( <op_type> [ , <op_type> ] ) ] <funcname> ( <argument_type> [, ...] )
  } [, ... ]

ALTER OPERATOR FAMILY <name> USING <index_method> DROP
  {  OPERATOR <strategy_number> ( <op_type>, <op_type> ) 
    | FUNCTION <support_number> [ ( <op_type> [ , <op_type> ] ) 
  } [, ... ]

ALTER OPERATOR FAMILY <name> USING <index_method> RENAME TO <new_name>

ALTER OPERATOR FAMILY <name> USING <index_method> OWNER TO <new_owner>

ALTER OPERATOR FAMILY <name> USING <index_method> SET SCHEMA <new_schema>
```

See [ALTER OPERATOR FAMILY](sql_commands/ALTER_OPERATOR_FAMILY.html) for more information.

## <a id="ae1903443"></a>ALTER PROTOCOL 

Changes the definition of a protocol.

```
ALTER PROTOCOL <name> RENAME TO <newname>

ALTER PROTOCOL <name> OWNER TO <newowner>
```

See [ALTER PROTOCOL](sql_commands/ALTER_PROTOCOL.html) for more information.

## <a id="ae19034513"></a>ALTER RESOURCE GROUP 

Changes the limits of a resource group.

```
ALTER RESOURCE GROUP <name> SET <group_attribute> <value>
```

See [ALTER RESOURCE GROUP](sql_commands/ALTER_RESOURCE_GROUP.html) for more information.

## <a id="ae1903451"></a>ALTER RESOURCE QUEUE 

Changes the limits of a resource queue.

```
ALTER RESOURCE QUEUE <name> WITH ( <queue_attribute>=<value> [, ... ] ) 
```

See [ALTER RESOURCE QUEUE](sql_commands/ALTER_RESOURCE_QUEUE.html) for more information.

## <a id="ae1903487"></a>ALTER ROLE 

Changes a database role \(user or group\).

```
ALTER ROLE <name> [ [ WITH ] <option> [ ... ] ]

where <option> can be:

    SUPERUSER | NOSUPERUSER
  | CREATEDB | NOCREATEDB
  | CREATEROLE | NOCREATEROLE
  | CREATEEXTTABLE | NOCREATEEXTTABLE  [ ( attribute='value' [, ...] )
     where attributes and values are:
       type='readable'|'writable'
       protocol='gpfdist'|'http'
  | INHERIT | NOINHERIT
  | LOGIN | NOLOGIN
  | REPLICATION | NOREPLICATION
  | CONNECTION LIMIT <connlimit>
  | [ ENCRYPTED | UNENCRYPTED ] PASSWORD '<password>'
  | VALID UNTIL '<timestamp>'

ALTER ROLE <name> RENAME TO <new_name>

ALTER ROLE { <name> | ALL } [ IN DATABASE <database_name> ] SET <configuration_parameter> { TO | = } { <value> | DEFAULT }
ALTER ROLE { <name> | ALL } [ IN DATABASE <database_name> ] SET <configuration_parameter> FROM CURRENT
ALTER ROLE { <name> | ALL } [ IN DATABASE <database_name> ] RESET <configuration_parameter>
ALTER ROLE { <name> | ALL } [ IN DATABASE <database_name> ] RESET ALL
ALTER ROLE <name> RESOURCE QUEUE {<queue_name> | NONE}
ALTER ROLE <name> RESOURCE GROUP {<group_name> | NONE}

```

See [ALTER ROLE](sql_commands/ALTER_ROLE.html) for more information.

## <a id="section_ssp_1hy_xgb"></a>ALTER RULE 

Changes the definition of a rule.

```
ALTER RULE name ON table\_name RENAME TO new\_name
```

See [ALTER RULE](sql_commands/ALTER_RULE.html) for more information.

## <a id="ae1903537"></a>ALTER SCHEMA 

Changes the definition of a schema.

```
ALTER SCHEMA <name> RENAME TO <newname>

ALTER SCHEMA <name> OWNER TO <newowner>
```

See [ALTER SCHEMA](sql_commands/ALTER_SCHEMA.html) for more information.

## <a id="ae1903545"></a>ALTER SEQUENCE 

Changes the definition of a sequence generator.

```
ALTER SEQUENCE [ IF EXISTS ] <name> [INCREMENT [ BY ] <increment>] 
     [MINVALUE <minvalue> | NO MINVALUE] 
     [MAXVALUE <maxvalue> | NO MAXVALUE] 
     [START [ WITH ] <start> ]
     [RESTART [ [ WITH ] <restart>] ]
     [CACHE <cache>] [[ NO ] CYCLE] 
     [OWNED BY {<table.column> | NONE}]

ALTER SEQUENCE [ IF EXISTS ] <name> OWNER TO <new_owner>

ALTER SEQUENCE [ IF EXISTS ] <name> RENAME TO <new_name>

ALTER SEQUENCE [ IF EXISTS ] <name> SET SCHEMA <new_schema>
```

See [ALTER SEQUENCE](sql_commands/ALTER_SEQUENCE.html) for more information.

## <a id="ae1903545s"></a>ALTER SERVER 

Changes the definition of a foreign server.

```
ALTER SERVER <server_name> [ VERSION '<new_version>' ]
    [ OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ] ) ]

ALTER SERVER <server_name> OWNER TO <new_owner>
                
ALTER SERVER <server_name> RENAME TO <new_name>
```

See [ALTER SERVER](sql_commands/ALTER_SERVER.html) for more information.

## <a id="ae1903563"></a>ALTER TABLE 

Changes the definition of a table.

```
ALTER TABLE [IF EXISTS] [ONLY] <name> 
    <action> [, ... ]

ALTER TABLE [IF EXISTS] [ONLY] <name> 
    RENAME [COLUMN] <column_name> TO <new_column_name>

ALTER TABLE [ IF EXISTS ] [ ONLY ] <name> 
    RENAME CONSTRAINT <constraint_name> TO <new_constraint_name>

ALTER TABLE [IF EXISTS] <name> 
    RENAME TO <new_name>

ALTER TABLE [IF EXISTS] <name> 
    SET SCHEMA <new_schema>

ALTER TABLE ALL IN TABLESPACE <name> [ OWNED BY <role_name> [, ... ] ]
    SET TABLESPACE <new_tablespace> [ NOWAIT ]

ALTER TABLE [IF EXISTS] [ONLY] <name> SET 
     WITH (REORGANIZE=true|false)
   | DISTRIBUTED BY ({<column_name> [<opclass>]} [, ... ] )
   | DISTRIBUTED RANDOMLY
   | DISTRIBUTED REPLICATED 

ALTER TABLE <name>
   [ ALTER PARTITION { <partition_name> | FOR (RANK(<number>)) 
   | FOR (<value>) } [...] ] <partition_action>

where <action> is one of:
                        
  ADD [COLUMN] <column_name data_type> [ DEFAULT <default_expr> ]
      [<column_constraint> [ ... ]]
      [ COLLATE <collation> ]
      [ ENCODING ( <storage_parameter> [,...] ) ]
  DROP [COLUMN] [IF EXISTS] <column_name> [RESTRICT | CASCADE]
  ALTER [COLUMN] <column_name> [ SET DATA ] TYPE <type> [COLLATE <collation>] [USING <expression>]
  ALTER [COLUMN] <column_name> SET DEFAULT <expression>
  ALTER [COLUMN] <column_name> DROP DEFAULT
  ALTER [COLUMN] <column_name> { SET | DROP } NOT NULL
  ALTER [COLUMN] <column_name> SET STATISTICS <integer>
  ALTER [COLUMN] column SET ( <attribute_option> = <value> [, ... ] )
  ALTER [COLUMN] column RESET ( <attribute_option> [, ... ] )
  ADD <table_constraint> [NOT VALID]
  ADD <table_constraint_using_index>
  VALIDATE CONSTRAINT <constraint_name>
  DROP CONSTRAINT [IF EXISTS] <constraint_name> [RESTRICT | CASCADE]
  DISABLE TRIGGER [<trigger_name> | ALL | USER]
  ENABLE TRIGGER [<trigger_name> | ALL | USER]
  CLUSTER ON <index_name>
  SET WITHOUT CLUSTER
  SET WITHOUT OIDS
  SET (<storage_parameter> = <value>)
  RESET (<storage_parameter> [, ... ])
  INHERIT <parent_table>
  NO INHERIT <parent_table>
  OF `type_name`
  NOT OF
  OWNER TO <new_owner>
  SET TABLESPACE <new_tablespace>
```

See [ALTER TABLE](sql_commands/ALTER_TABLE.html) for more information.

## <a id="ae1903767"></a>ALTER TABLESPACE 

Changes the definition of a tablespace.

```
ALTER TABLESPACE <name> RENAME TO <new_name>

ALTER TABLESPACE <name> OWNER TO <new_owner>

ALTER TABLESPACE <name> SET ( <tablespace_option> = <value> [, ... ] )

ALTER TABLESPACE <name> RESET ( <tablespace_option> [, ... ] )


```

See [ALTER TABLESPACE](sql_commands/ALTER_TABLESPACE.html) for more information.

## <a id="fixme"></a>ALTER TEXT SEARCH CONFIGURATION 

Changes the definition of a text search configuration.

```
ALTER TEXT SEARCH CONFIGURATION <name>
    ALTER MAPPING FOR <token_type> [, ... ] WITH <dictionary_name> [, ... ]
ALTER TEXT SEARCH CONFIGURATION <name>
    ALTER MAPPING REPLACE <old_dictionary> WITH <new_dictionary>
ALTER TEXT SEARCH CONFIGURATION <name>
    ALTER MAPPING FOR <token_type> [, ... ] REPLACE <old_dictionary> WITH <new_dictionary>
ALTER TEXT SEARCH CONFIGURATION <name>
    DROP MAPPING [ IF EXISTS ] FOR <token_type> [, ... ]
ALTER TEXT SEARCH CONFIGURATION <name> RENAME TO <new_name>
ALTER TEXT SEARCH CONFIGURATION <name> OWNER TO <new_owner>
ALTER TEXT SEARCH CONFIGURATION <name> SET SCHEMA <new_schema>
```

See [ALTER TEXT SEARCH CONFIGURATION](sql_commands/ALTER_TEXT_SEARCH_CONFIGURATION.html) for more information.

## <a id="fixme"></a>ALTER TEXT SEARCH DICTIONARY 

Changes the definition of a text search dictionary.

```
ALTER TEXT SEARCH DICTIONARY <name> (
    <option> [ = <value> ] [, ... ]
)
ALTER TEXT SEARCH DICTIONARY <name> RENAME TO <new_name>
ALTER TEXT SEARCH DICTIONARY <name> OWNER TO <new_owner>
ALTER TEXT SEARCH DICTIONARY <name> SET SCHEMA <new_schema>
```

See [ALTER TEXT SEARCH DICTIONARY](sql_commands/ALTER_TEXT_SEARCH_DICTIONARY.html) for more information.

## <a id="fixme"></a>ALTER TEXT SEARCH PARSER 

Changes the definition of a text search parser.

```
ALTER TEXT SEARCH PARSER <name> RENAME TO <new_name>
ALTER TEXT SEARCH PARSER <name> SET SCHEMA <new_schema>
```

See [ALTER TEXT SEARCH PARSER](sql_commands/ALTER_TEXT_SEARCH_PARSER.html) for more information.

## <a id="fixme"></a>ALTER TEXT SEARCH TEMPLATE 

Changes the definition of a text search template.

```
ALTER TEXT SEARCH TEMPLATE <name> RENAME TO <new_name>
ALTER TEXT SEARCH TEMPLATE <name> SET SCHEMA <new_schema>
```

See [ALTER TEXT SEARCH TEMPLATE](sql_commands/ALTER_TEXT_SEARCH_TEMPLATE.html) for more information.

## <a id="ae1903781"></a>ALTER TYPE 

Changes the definition of a data type.

```

ALTER TYPE <name> <action> [, ... ]
ALTER TYPE <name> OWNER TO <new_owner>
ALTER TYPE <name> RENAME ATTRIBUTE <attribute_name> TO <new_attribute_name> [ CASCADE | RESTRICT ]
ALTER TYPE <name> RENAME TO <new_name>
ALTER TYPE <name> SET SCHEMA <new_schema>
ALTER TYPE <name> ADD VALUE [ IF NOT EXISTS ] <new_enum_value> [ { BEFORE | AFTER } <existing_enum_value> ]
ALTER TYPE <name> SET DEFAULT ENCODING ( <storage_directive> )

where <action> is one of:
  
  ADD ATTRIBUTE <attribute_name> <data_type> [ COLLATE <collation> ] [ CASCADE | RESTRICT ]
  DROP ATTRIBUTE [ IF EXISTS ] <attribute_name> [ CASCADE | RESTRICT ]
  ALTER ATTRIBUTE <attribute_name> [ SET DATA ] TYPE <data_type> [ COLLATE <collation> ] [ CASCADE | RESTRICT ]

```

See [ALTER TYPE](sql_commands/ALTER_TYPE.html) for more information.

## <a id="ae1903787"></a>ALTER USER 

Changes the definition of a database role \(user\).

```
ALTER USER <name> RENAME TO <newname>

ALTER USER <name> SET <config_parameter> {TO | =} {<value> | DEFAULT}

ALTER USER <name> RESET <config_parameter>

ALTER USER <name> RESOURCE QUEUE {<queue_name> | NONE}

ALTER USER <name> RESOURCE GROUP {<group_name> | NONE}

ALTER USER <name> [ [WITH] <option> [ ... ] ]
```

See [ALTER USER](sql_commands/ALTER_USER.html) for more information.

## <a id="ae1903787u"></a>ALTER USER MAPPING 

Changes the definition of a user mapping for a foreign server.

```
ALTER USER MAPPING FOR { <username> | USER | CURRENT_USER | PUBLIC }
    SERVER <servername>
    OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ] )
```

See [ALTER USER MAPPING](sql_commands/ALTER_USER_MAPPING.html) for more information.

## <a id="fixme"></a>ALTER VIEW 

Changes properties of a view.

```
ALTER VIEW [ IF EXISTS ] <name> ALTER [ COLUMN ] <column_name> SET DEFAULT <expression>

ALTER VIEW [ IF EXISTS ] <name> ALTER [ COLUMN ] <column_name> DROP DEFAULT

ALTER VIEW [ IF EXISTS ] <name> OWNER TO <new_owner>

ALTER VIEW [ IF EXISTS ] <name> RENAME TO <new_name>

ALTER VIEW [ IF EXISTS ] <name> SET SCHEMA <new_schema>

ALTER VIEW [ IF EXISTS ] <name> SET ( <view_option_name> [= <view_option_value>] [, ... ] )

ALTER VIEW [ IF EXISTS ] <name> RESET ( <view_option_name> [, ... ] )
```

See [ALTER VIEW](sql_commands/ALTER_VIEW.html) for more information.

## <a id="ae1903817"></a>ANALYZE 

Collects statistics about a database.

```
ANALYZE [VERBOSE] [<table> [ (<column> [, ...] ) ]]

ANALYZE [VERBOSE] {<root_partition_table_name>|<leaf_partition_table_name>} [ (<column> [, ...] )] 

ANALYZE [VERBOSE] ROOTPARTITION {ALL | <root_partition_table_name> [ (<column> [, ...] )]}
```

See [ANALYZE](sql_commands/ANALYZE.html) for more information.

## <a id="ae1903823"></a>BEGIN 

Starts a transaction block.

```
BEGIN [WORK | TRANSACTION] [<transaction_mode>]
```

See [BEGIN](sql_commands/BEGIN.html) for more information.

## <a id="ae1903829"></a>CHECKPOINT 

Forces a transaction log checkpoint.

```
CHECKPOINT
```

See [CHECKPOINT](sql_commands/CHECKPOINT.html) for more information.

## <a id="ae1903835"></a>CLOSE 

Closes a cursor.

```
CLOSE <cursor_name>
```

See [CLOSE](sql_commands/CLOSE.html) for more information.

## <a id="ae1903841"></a>CLUSTER 

Physically reorders a heap storage table on disk according to an index. Not a recommended operation in Greenplum Database.

```
CLUSTER <indexname> ON <tablename>

CLUSTER [VERBOSE] <tablename> [ USING index_name ]

CLUSTER [VERBOSE]
```

See [CLUSTER](sql_commands/CLUSTER.html) for more information.

## <a id="ae1903851"></a>COMMENT 

Defines or changes the comment of an object.

```
COMMENT ON
{ TABLE <object_name |
  COLUMN <relation_name.column_name |
  AGGREGATE <agg_name (<agg_signature>) |
  CAST (<source_type AS <target_type>) |
  COLLATION <object_name
  CONSTRAINT <constraint_name ON <table_name> |
  CONVERSION <object_name |
  DATABASE <object_name |
  DOMAIN <object_name |
  EXTENSION <object_name |
  FOREIGN DATA WRAPPER <object_name |
  FOREIGN TABLE <object_name |
  FUNCTION <func_name ([[<argmode>] [<argname>] <argtype> [, ...]]) |
  INDEX <object_name |
  LARGE OBJECT <large_object_oid |
  MATERIALIZED VIEW <object_name |
  OPERATOR <operator_name (<left_type>, <right_type>) |
  OPERATOR CLASS <object_name USING <index_method> |
  [PROCEDURAL] LANGUAGE <object_name |
  RESOURCE GROUP <object_name |
  RESOURCE QUEUE <object_name |
  ROLE <object_name |
  RULE <rule_name ON <table_name> |
  SCHEMA <object_name |
  SEQUENCE <object_name |
  SERVER <object_name |
  TABLESPACE <object_name |
  TEXT SEARCH CONFIGURATION <object_name |
  TEXT SEARCH DICTIONARY <object_name |
  TEXT SEARCH PARSER <object_name |
  TEXT SEARCH TEMPLATE <object_name |
  TRIGGER <trigger_name ON <table_name> |
  TYPE <object_name |
  VIEW <object_name } 
IS 'text'
```

See [COMMENT](sql_commands/COMMENT.html) for more information.

## <a id="ae1903907"></a>COMMIT 

Commits the current transaction.

```
COMMIT [WORK | TRANSACTION]
```

See [COMMIT](sql_commands/COMMIT.html) for more information.

## <a id="ae1903913"></a>COPY 

Copies data between a file and a table.

```
COPY <table_name> [(<column_name> [, ...])] 
     FROM {'<filename>' | PROGRAM '<command>' | STDIN}
     [ [ WITH ] ( <option> [, ...] ) ]
     [ ON SEGMENT ]

COPY { <table_name> [(<column_name> [, ...])] | (<query>)} 
     TO {'<filename>' | PROGRAM '<command>' | STDOUT}
     [ [ WITH ] ( <option> [, ...] ) ]
     [ ON SEGMENT ]
```

See [COPY](sql_commands/COPY.html) for more information.

## <a id="ae1903961"></a>CREATE AGGREGATE 

Defines a new aggregate function.

```
CREATE AGGREGATE <name> ( [ <argmode> ] [ <argname> ] <arg_data_type> [ , ... ] ) (
    SFUNC = <statefunc>,
    STYPE = <state_data_type>
    [ , SSPACE = <state_data_size> ]
    [ , FINALFUNC = <ffunc> ]
    [ , FINALFUNC_EXTRA ]
    [ , COMBINEFUNC = <combinefunc> ]
    [ , SERIALFUNC = <serialfunc> ]
    [ , DESERIALFUNC = <deserialfunc> ]
    [ , INITCOND = <initial_condition> ]
    [ , MSFUNC = <msfunc> ]
    [ , MINVFUNC = <minvfunc> ]
    [ , MSTYPE = <mstate_data_type> ]
    [ , MSSPACE = <mstate_data_size> ]
    [ , MFINALFUNC = <mffunc> ]
    [ , MFINALFUNC_EXTRA ]
    [ , MINITCOND = <minitial_condition> ]
    [ , SORTOP = <sort_operator> ]
  )
  
  CREATE AGGREGATE <name> ( [ [ <argmode> ] [ <argname> ] <arg_data_type> [ , ... ] ]
      ORDER BY [ <argmode> ] [ <argname> ] <arg_data_type> [ , ... ] ) (
    SFUNC = <statefunc>,
    STYPE = <state_data_type>
    [ , SSPACE = <state_data_size> ]
    [ , FINALFUNC = <ffunc> ]
    [ , FINALFUNC_EXTRA ]
    [ , COMBINEFUNC = <combinefunc> ]
    [ , SERIALFUNC = <serialfunc> ]
    [ , DESERIALFUNC = <deserialfunc> ]
    [ , INITCOND = <initial_condition> ]
    [ , HYPOTHETICAL ]
  )
  
  or the old syntax
  
  CREATE AGGREGATE <name> (
    BASETYPE = <base_type>,
    SFUNC = <statefunc>,
    STYPE = <state_data_type>
    [ , SSPACE = <state_data_size> ]
    [ , FINALFUNC = <ffunc> ]
    [ , FINALFUNC_EXTRA ]
    [ , COMBINEFUNC = <combinefunc> ]
    [ , SERIALFUNC = <serialfunc> ]
    [ , DESERIALFUNC = <deserialfunc> ]
    [ , INITCOND = <initial_condition> ]
    [ , MSFUNC = <msfunc> ]
    [ , MINVFUNC = <minvfunc> ]
    [ , MSTYPE = <mstate_data_type> ]
    [ , MSSPACE = <mstate_data_size> ]
    [ , MFINALFUNC = <mffunc> ]
    [ , MFINALFUNC_EXTRA ]
    [ , MINITCOND = <minitial_condition> ]
    [ , SORTOP = <sort_operator> ]
  )
```

See [CREATE AGGREGATE](sql_commands/CREATE_AGGREGATE.html) for more information.

## <a id="ae1903979"></a>CREATE CAST 

Defines a new cast.

```
CREATE CAST (<sourcetype> AS <targettype>) 
       WITH FUNCTION <funcname> (<argtype> [, ...]) 
       [AS ASSIGNMENT | AS IMPLICIT]

CREATE CAST (<sourcetype> AS <targettype>)
       WITHOUT FUNCTION 
       [AS ASSIGNMENT | AS IMPLICIT]

CREATE CAST (<sourcetype> AS <targettype>)
       WITH INOUT 
       [AS ASSIGNMENT | AS IMPLICIT]
```

See [CREATE CAST](sql_commands/CREATE_CAST.html) for more information.

## <a id="ae1903993"></a>CREATE COLLATION 

Defines a new collation using the specified operating system locale settings, or by copying an existing collation.

```
CREATE COLLATION <name> (    
    [ LOCALE = <locale>, ]    
    [ LC_COLLATE = <lc_collate>, ]    
    [ LC_CTYPE = <lc_ctype> ])

CREATE COLLATION <name> FROM <existing_collation>
```

See [CREATE COLLATION](sql_commands/CREATE_COLLATION.html) for more information.

## <a id="section_wct_xx3_sgb"></a>CREATE CONVERSION 

Defines a new encoding conversion.

```
CREATE [DEFAULT] CONVERSION <name> FOR <source_encoding> TO 
     <dest_encoding> FROM <funcname>
```

See [CREATE CONVERSION](sql_commands/CREATE_CONVERSION.html) for more information.

## <a id="ae1903999"></a>CREATE DATABASE 

Creates a new database.

```
CREATE DATABASE name [ [WITH] [OWNER [=] <user_name>]
                     [TEMPLATE [=] <template>]
                     [ENCODING [=] <encoding>]
                     [LC_COLLATE [=] <lc_collate>]
                     [LC_CTYPE [=] <lc_ctype>]
                     [TABLESPACE [=] <tablespace>]
                     [CONNECTION LIMIT [=] connlimit ] ]
```

See [CREATE DATABASE](sql_commands/CREATE_DATABASE.html) for more information.

## <a id="ae1904013"></a>CREATE DOMAIN 

Defines a new domain.

```
CREATE DOMAIN <name> [AS] <data_type> [DEFAULT <expression>]
       [ COLLATE <collation> ] 
       [ CONSTRAINT <constraint_name>
       | NOT NULL | NULL 
       | CHECK (<expression>) [...]]
```

See [CREATE DOMAIN](sql_commands/CREATE_DOMAIN.html) for more information.

## <a id="fixme"></a>CREATE EXTENSION 

Registers an extension in a Greenplum database.

```
CREATE EXTENSION [ IF NOT EXISTS ] <extension_name>
  [ WITH ] [ SCHEMA <schema_name> ]
           [ VERSION <version> ]
           [ FROM <old_version> ]
           [ CASCADE ]
```

See [CREATE EXTENSION](sql_commands/CREATE_EXTENSION.html) for more information.

## <a id="ae1904025"></a>CREATE EXTERNAL TABLE 

Defines a new external table.

```
CREATE [READABLE] EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>     
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION ('file://<seghost>[:<port>]/<path>/<file>' [, ...])
       | ('gpfdist://<filehost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
           [, ...]
       | ('gpfdists://<filehost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
           [, ...])
       | ('pxf://<path-to-data>?PROFILE=<profile_name>[&SERVER=<server_name>][&<custom-option>=<value>[...]]'))
       | ('s3://<S3_endpoint>[:<port>]/<bucket_name>/[<S3_prefix>] [region=<S3-region>] [config=<config_file> | config_server=<url>]')
     [ON MASTER]
     FORMAT 'TEXT' 
           [( [HEADER]
              [DELIMITER [AS] '<delimiter>' | 'OFF']
              [NULL [AS] '<null string>']
              [ESCAPE [AS] '<escape>' | 'OFF']
              [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
              [FILL MISSING FIELDS] )]
          | 'CSV'
           [( [HEADER]
              [QUOTE [AS] '<quote>'] 
              [DELIMITER [AS] '<delimiter>']
              [NULL [AS] '<null string>']
              [FORCE NOT NULL <column> [, ...]]
              [ESCAPE [AS] '<escape>']
              [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
              [FILL MISSING FIELDS] )]
          | 'CUSTOM' (Formatter=<<formatter_specifications>>)
    [ OPTIONS ( <key> '<value>' [, ...] ) ]
    [ ENCODING '<encoding>' ]
      [ [LOG ERRORS [PERSISTENTLY]] SEGMENT REJECT LIMIT <count>
      [ROWS | PERCENT] ]

CREATE [READABLE] EXTERNAL WEB [TEMPORARY | TEMP] TABLE <table_name>     
   ( <column_name> <data_type> [, ...] | LIKE <other_table >)
      LOCATION ('http://<webhost>[:<port>]/<path>/<file>' [, ...])
    | EXECUTE '<command>' [ON ALL 
                          | MASTER
                          | <number_of_segments>
                          | HOST ['<segment_hostname>'] 
                          | SEGMENT <segment_id> ]
      FORMAT 'TEXT' 
            [( [HEADER]
               [DELIMITER [AS] '<delimiter>' | 'OFF']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF']
               [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
               [FILL MISSING FIELDS] )]
           | 'CSV'
            [( [HEADER]
               [QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE NOT NULL <column> [, ...]]
               [ESCAPE [AS] '<escape>']
               [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
               [FILL MISSING FIELDS] )]
           | 'CUSTOM' (Formatter=<<formatter specifications>>)
     [ OPTIONS ( <key> '<value>' [, ...] ) ]
     [ ENCODING '<encoding>' ]
     [ [LOG ERRORS [PERSISTENTLY]] SEGMENT REJECT LIMIT <count>
       [ROWS | PERCENT] ]

CREATE WRITABLE EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION('gpfdist://<outputhost>[:<port>]/<filename>[#transform=<trans_name>]'
          [, ...])
      | ('gpfdists://<outputhost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
          [, ...])
      FORMAT 'TEXT' 
               [( [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF'] )]
          | 'CSV'
               [([QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE QUOTE <column> [, ...]] | * ]
               [ESCAPE [AS] '<escape>'] )]

           | 'CUSTOM' (Formatter=<<formatter specifications>>)
    [ OPTIONS ( <key> '<value>' [, ...] ) ]
    [ ENCODING '<write_encoding>' ]
    [ DISTRIBUTED BY ({<column> [<opclass>]}, [ ... ] ) | DISTRIBUTED RANDOMLY ]

CREATE WRITABLE EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION('s3://<S3_endpoint>[:<port>]/<bucket_name>/[<S3_prefix>] [region=<S3-region>] [config=<config_file> | config_server=<url>]')
      [ON MASTER]
      FORMAT 'TEXT' 
               [( [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF'] )]
          | 'CSV'
               [([QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE QUOTE <column> [, ...]] | * ]
               [ESCAPE [AS] '<escape>'] )]

CREATE WRITABLE EXTERNAL WEB [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table> )
    EXECUTE '<command>' [ON ALL]
    FORMAT 'TEXT' 
               [( [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF'] )]
          | 'CSV'
               [([QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE QUOTE <column> [, ...]] | * ]
               [ESCAPE [AS] '<escape>'] )]
           | 'CUSTOM' (Formatter=<<formatter specifications>>)
    [ OPTIONS ( <key> '<value>' [, ...] ) ]
    [ ENCODING '<write_encoding>' ]
    [ DISTRIBUTED BY ({<column> [<opclass>]}, [ ... ] ) | DISTRIBUTED RANDOMLY ]
```

See [CREATE EXTERNAL TABLE](sql_commands/CREATE_EXTERNAL_TABLE.html) for more information.

## <a id="ae1904203w"></a>CREATE FOREIGN DATA WRAPPER 

Defines a new foreign-data wrapper.

```
CREATE FOREIGN DATA WRAPPER <name>
    [ HANDLER <handler_function> | NO HANDLER ]
    [ VALIDATOR <validator_function> | NO VALIDATOR ]
    [ OPTIONS ( [ mpp_execute { 'master' | 'any' | 'all segments' } [, ] ] <option> '<value>' [, ... ] ) ]
```

See [CREATE FOREIGN DATA WRAPPER](sql_commands/CREATE_FOREIGN_DATA_WRAPPER.html) for more information.

## <a id="ae1904203t"></a>CREATE FOREIGN TABLE 

Defines a new foreign table.

```
CREATE FOREIGN TABLE [ IF NOT EXISTS ] <table_name> ( [
    <column_name> <data_type> [ OPTIONS ( <option> '<value>' [, ... ] ) ] [ COLLATE <collation> ] [ <column_constraint> [ ... ] ]
      [, ... ]
] )
    SERVER <server_name>
  [ OPTIONS ( [ mpp_execute { 'master' | 'any' | 'all segments' } [, ] ] <option> '<value>' [, ... ] ) ]
```

See [CREATE FOREIGN TABLE](sql_commands/CREATE_FOREIGN_TABLE.html) for more information.

## <a id="ae1904203"></a>CREATE FUNCTION 

Defines a new function.

```
CREATE [OR REPLACE] FUNCTION <name>    
    ( [ [<argmode>] [<argname>] <argtype> [ { DEFAULT | = } <default_expr> ] [, ...] ] )
      [ RETURNS <rettype> 
        | RETURNS TABLE ( <column_name> <column_type> [, ...] ) ]
    { LANGUAGE <langname>
    | WINDOW
    | IMMUTABLE | STABLE | VOLATILE | [NOT] LEAKPROOF
    | CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT
    | NO SQL | CONTAINS SQL | READS SQL DATA | MODIFIES SQL
    | [EXTERNAL] SECURITY INVOKER | [EXTERNAL] SECURITY DEFINER
    | EXECUTE ON { ANY | MASTER | ALL SEGMENTS | INITPLAN }
    | COST <execution_cost>
    | SET <configuration_parameter> { TO <value> | = <value> | FROM CURRENT }
    | AS '<definition>'
    | AS '<obj_file>', '<link_symbol>' } ...
    [ WITH ({ DESCRIBE = describe_function
           } [, ...] ) ]
```

See [CREATE FUNCTION](sql_commands/CREATE_FUNCTION.html) for more information.

## <a id="ae1904233"></a>CREATE GROUP 

Defines a new database role.

```
CREATE GROUP <name> [[WITH] <option> [ ... ]]
```

See [CREATE GROUP](sql_commands/CREATE_GROUP.html) for more information.

## <a id="ae1904269"></a>CREATE INDEX 

Defines a new index.

```
CREATE [UNIQUE] INDEX [<name>] ON <table_name> [USING <method>]
       ( {<column_name> | (<expression>)} [COLLATE <parameter>] [<opclass>] [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] )
       [ WITH ( <storage_parameter> = <value> [, ... ] ) ]
       [ TABLESPACE <tablespace> ]
       [ WHERE <predicate> ]
```

See [CREATE INDEX](sql_commands/CREATE_INDEX.html) for more information.

## <a id="ae1904285"></a>CREATE LANGUAGE 

Defines a new procedural language.

```
CREATE [ OR REPLACE ] [ PROCEDURAL ] LANGUAGE <name>

CREATE [ OR REPLACE ] [ TRUSTED ] [ PROCEDURAL ] LANGUAGE <name>
    HANDLER <call_handler> [ INLINE <inline_handler> ] 
   [ VALIDATOR <valfunction> ]
            
```

See [CREATE LANGUAGE](sql_commands/CREATE_LANGUAGE.html) for more information.

## <a id="fixme"></a>CREATE MATERIALIZED VIEW 

Defines a new materialized view.

```
CREATE MATERIALIZED VIEW <table_name>
    [ (<column_name> [, ...] ) ]
    [ WITH ( <storage_parameter> [= <value>] [, ... ] ) ]
    [ TABLESPACE <tablespace_name> ]
    AS <query>
    [ WITH [ NO ] DATA ]
    [DISTRIBUTED {| BY <column> [<opclass>], [ ... ] | RANDOMLY | REPLICATED }]
```

See [CREATE MATERIALIZED VIEW](sql_commands/CREATE_MATERIALIZED_VIEW.html) for more information.

## <a id="ae1904295"></a>CREATE OPERATOR 

Defines a new operator.

```
CREATE OPERATOR <name> ( 
       PROCEDURE = <funcname>
       [, LEFTARG = <lefttype>] [, RIGHTARG = <righttype>]
       [, COMMUTATOR = <com_op>] [, NEGATOR = <neg_op>]
       [, RESTRICT = <res_proc>] [, JOIN = <join_proc>]
       [, HASHES] [, MERGES] )
```

See [CREATE OPERATOR](sql_commands/CREATE_OPERATOR.html) for more information.

## <a id="ae1904315"></a>CREATE OPERATOR CLASS 

Defines a new operator class.

```
CREATE OPERATOR CLASS <name> [DEFAULT] FOR TYPE <data_type>  
  USING <index_method> [ FAMILY <family_name> ] AS 
  { OPERATOR <strategy_number> <operator_name> [ ( <op_type>, <op_type> ) ] [ FOR SEARCH | FOR ORDER BY <sort_family_name> ]
  | FUNCTION <support_number> <funcname> (<argument_type> [, ...] )
  | STORAGE <storage_type>
  } [, ... ]
```

See [CREATE OPERATOR CLASS](sql_commands/CREATE_OPERATOR_CLASS.html) for more information.

## <a id="fixme"></a>CREATE OPERATOR FAMILY 

Defines a new operator family.

```
CREATE OPERATOR FAMILY <name>  USING <index_method>  
```

See [CREATE OPERATOR FAMILY](sql_commands/CREATE_OPERATOR_FAMILY.html) for more information.

## <a id="fixme"></a>CREATE PROTOCOL 

Registers a custom data access protocol that can be specified when defining a Greenplum Database external table.

```
CREATE [TRUSTED] PROTOCOL <name> (
   [readfunc='<read_call_handler>'] [, writefunc='<write_call_handler>']
   [, validatorfunc='<validate_handler>' ])
```

See [CREATE PROTOCOL](sql_commands/CREATE_PROTOCOL.html) for more information.

## <a id="ae19043333"></a>CREATE RESOURCE GROUP 

Defines a new resource group.

```
CREATE RESOURCE GROUP <name> WITH (<group_attribute>=<value> [, ... ])
```

See [CREATE RESOURCE GROUP](sql_commands/CREATE_RESOURCE_GROUP.html) for more information.

## <a id="ae1904333"></a>CREATE RESOURCE QUEUE 

Defines a new resource queue.

```
CREATE RESOURCE QUEUE <name> WITH (<queue_attribute>=<value> [, ... ])
```

See [CREATE RESOURCE QUEUE](sql_commands/CREATE_RESOURCE_QUEUE.html) for more information.

## <a id="ae1904361"></a>CREATE ROLE 

Defines a new database role \(user or group\).

```
CREATE ROLE <name> [[WITH] <option> [ ... ]]
```

See [CREATE ROLE](sql_commands/CREATE_ROLE.html) for more information.

## <a id="ae1904407"></a>CREATE RULE 

Defines a new rewrite rule.

```
CREATE [OR REPLACE] RULE <name> AS ON <event>
  TO <table_name> [WHERE <condition>] 
  DO [ALSO | INSTEAD] { NOTHING | <command> | (<command>; <command> 
  ...) }
```

See [CREATE RULE](sql_commands/CREATE_RULE.html) for more information.

## <a id="ae1904417"></a>CREATE SCHEMA 

Defines a new schema.

```
CREATE SCHEMA <schema_name> [AUTHORIZATION <username>] 
   [<schema_element> [ ... ]]

CREATE SCHEMA AUTHORIZATION <rolename> [<schema_element> [ ... ]]

CREATE SCHEMA IF NOT EXISTS <schema_name> [ AUTHORIZATION <user_name> ]

CREATE SCHEMA IF NOT EXISTS AUTHORIZATION <user_name>

```

See [CREATE SCHEMA](sql_commands/CREATE_SCHEMA.html) for more information.

## <a id="ae1904425"></a>CREATE SEQUENCE 

Defines a new sequence generator.

```
CREATE [TEMPORARY | TEMP] SEQUENCE <name>
       [INCREMENT [BY] <value>] 
       [MINVALUE <minvalue> | NO MINVALUE] 
       [MAXVALUE <maxvalue> | NO MAXVALUE] 
       [START [ WITH ] <start>] 
       [CACHE <cache>] 
       [[NO] CYCLE] 
       [OWNED BY { <table>.<column> | NONE }]
```

See [CREATE SEQUENCE](sql_commands/CREATE_SEQUENCE.html) for more information.

## <a id="ae1904425s"></a>CREATE SERVER 

Defines a new foreign server.

```
CREATE SERVER <server_name> [ TYPE '<server_type>' ] [ VERSION '<server_version>' ]
    FOREIGN DATA WRAPPER <fdw_name>
    [ OPTIONS ( [ mpp_execute { 'master' | 'any' | 'all segments' } [, ] ]
                [ num_segments '<num>' [, ] ]
                [ <option> '<value>' [, ... ]] ) ]
```

See [CREATE SERVER](sql_commands/CREATE_SERVER.html) for more information.

## <a id="ae1904445"></a>CREATE TABLE 

Defines a new table.

```

CREATE [ [GLOBAL | LOCAL] {TEMPORARY | TEMP } | UNLOGGED] TABLE [IF NOT EXISTS] 
  <table_name> ( 
  [ { <column_name> <data_type> [ COLLATE <collation> ] [<column_constraint> [ ... ] ]
[ ENCODING ( <storage_directive> [, ...] ) ]
    | <table_constraint>
    | LIKE <source_table> [ <like_option> ... ] }
    | [ <column_reference_storage_directive> [, ...]
    [, ... ]
] )
[ INHERITS ( <parent_table> [, ... ] ) ]
[ WITH ( <storage_parameter> [=<value>] [, ... ] ) ]
[ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
[ TABLESPACE <tablespace_name> ]
[ DISTRIBUTED BY (<column> [<opclass>], [ ... ] ) 
       | DISTRIBUTED RANDOMLY | DISTRIBUTED REPLICATED ]

{ --partitioned table using SUBPARTITION TEMPLATE
[ PARTITION BY <partition_type> (<column>) 
  {  [ SUBPARTITION BY <partition_type> (<column1>) 
       SUBPARTITION TEMPLATE ( <template_spec> ) ]
          [ SUBPARTITION BY partition_type (<column2>) 
            SUBPARTITION TEMPLATE ( <template_spec> ) ]
              [...]  }
  ( <partition_spec> ) ]
} |

{ -- partitioned table without SUBPARTITION TEMPLATE
[ PARTITION BY <partition_type> (<column>)
   [ SUBPARTITION BY <partition_type> (<column1>) ]
      [ SUBPARTITION BY <partition_type> (<column2>) ]
         [...]
  ( <partition_spec>
     [ ( <subpartition_spec_column1>
          [ ( <subpartition_spec_column2>
               [...] ) ] ) ],
  [ <partition_spec>
     [ ( <subpartition_spec_column1>
        [ ( <subpartition_spec_column2>
             [...] ) ] ) ], ]
    [...]
  ) ]
}

CREATE [ [GLOBAL | LOCAL] {TEMPORARY | TEMP} | UNLOGGED ] TABLE [IF NOT EXISTS] 
   <table_name>
    OF <type_name> [ (
  { <column_name> WITH OPTIONS [ <column_constraint> [ ... ] ]
    | <table_constraint> } 
    [, ... ]
) ]
[ WITH ( <storage_parameter> [=<value>] [, ... ] ) ]
[ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
[ TABLESPACE <tablespace_name> ]

```

See [CREATE TABLE](sql_commands/CREATE_TABLE.html) for more information.

## <a id="ae1904649"></a>CREATE TABLE AS 

Defines a new table from the results of a query.

```
CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE <table_name>
        [ (<column_name> [, ...] ) ]
        [ WITH ( <storage_parameter> [= <value>] [, ... ] ) | WITHOUT OIDS ]
        [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
        [ TABLESPACE <tablespace_name> ]
        AS <query>
        [ WITH [ NO ] DATA ]
        [ DISTRIBUTED BY (column [, ... ] ) | DISTRIBUTED RANDOMLY | DISTRIBUTED REPLICATED ]
      
```

See [CREATE TABLE AS](sql_commands/CREATE_TABLE_AS.html) for more information.

## <a id="ae1904683"></a>CREATE TABLESPACE 

Defines a new tablespace.

```
CREATE TABLESPACE <tablespace_name> [OWNER <username>]  LOCATION '</path/to/dir>' 
   [WITH (content<ID_1>='</path/to/dir1>'[, content<ID_2>='</path/to/dir2>' ... ])]
```

See [CREATE TABLESPACE](sql_commands/CREATE_TABLESPACE.html) for more information.

## <a id="fixme"></a>CREATE TEXT SEARCH CONFIGURATION 

Defines a new text search configuration.

```
CREATE TEXT SEARCH CONFIGURATION <name> (
    PARSER = <parser_name> |
    COPY = <source_config>
)
```

See [CREATE TEXT SEARCH CONFIGURATION](sql_commands/CREATE_TEXT_SEARCH_CONFIGURATION.html) for more information.

## <a id="fixme"></a>CREATE TEXT SEARCH DICTIONARY 

Defines a new text search dictionary.

```
CREATE TEXT SEARCH DICTIONARY <name> (
    TEMPLATE = <template>
    [, <option> = <value> [, ... ]]
)
```

See [CREATE TEXT SEARCH DICTIONARY](sql_commands/CREATE_TEXT_SEARCH_DICTIONARY.html) for more information.

## <a id="fixme"></a>CREATE TEXT SEARCH PARSER 

Defines a new text search parser.

```
CREATE TEXT SEARCH PARSER name (
    START = start_function ,
    GETTOKEN = gettoken_function ,
    END = end_function ,
    LEXTYPES = lextypes_function
    [, HEADLINE = headline_function ]
)
```

See [CREATE TEXT SEARCH PARSER](sql_commands/CREATE_TEXT_SEARCH_PARSER.html) for more information.

## <a id="fixme"></a>CREATE TEXT SEARCH TEMPLATE 

Defines a new text search template.

```
CREATE TEXT SEARCH TEMPLATE <name> (
    [ INIT = <init_function> , ]
    LEXIZE = <lexize_function>
)
```

See [CREATE TEXT SEARCH TEMPLATE](sql_commands/CREATE_TEXT_SEARCH_TEMPLATE.html) for more information.

## <a id="ae1904701"></a>CREATE TYPE 

Defines a new data type.

```
CREATE TYPE <name> AS 
    ( <attribute_name> <data_type> [ COLLATE <collation> ] [, ... ] ] )

CREATE TYPE <name> AS ENUM 
    ( [ '<label>' [, ... ] ] )

CREATE TYPE <name> AS RANGE (
    SUBTYPE = <subtype>
    [ , SUBTYPE_OPCLASS = <subtype_operator_class> ]
    [ , COLLATION = <collation> ]
    [ , CANONICAL = <canonical_function> ]
    [ , SUBTYPE_DIFF = <subtype_diff_function> ]
)

CREATE TYPE <name> (
    INPUT = <input_function>,
    OUTPUT = <output_function>
    [, RECEIVE = <receive_function>]
    [, SEND = <send_function>]
    [, TYPMOD_IN = <type_modifier_input_function> ]
    [, TYPMOD_OUT = <type_modifier_output_function> ]
    [, INTERNALLENGTH = {<internallength> | VARIABLE}]
    [, PASSEDBYVALUE]
    [, ALIGNMENT = <alignment>]
    [, STORAGE = <storage>]
    [, LIKE = <like_type>
    [, CATEGORY = <category>]
    [, PREFERRED = <preferred>]
    [, DEFAULT = <default>]
    [, ELEMENT = <element>]
    [, DELIMITER = <delimiter>]
    [, COLLATABLE = <collatable>]
    [, COMPRESSTYPE = <compression_type>]
    [, COMPRESSLEVEL = <compression_level>]
    [, BLOCKSIZE = <blocksize>] )

CREATE TYPE <name>
```

See [CREATE TYPE](sql_commands/CREATE_TYPE.html) for more information.

## <a id="ae1904741"></a>CREATE USER 

Defines a new database role with the `LOGIN` privilege by default.

```
CREATE USER <name> [[WITH] <option> [ ... ]]
```

See [CREATE USER](sql_commands/CREATE_USER.html) for more information.

## <a id="ae1904741u"></a>CREATE USER MAPPING 

Defines a new mapping of a user to a foreign server.

```
CREATE USER MAPPING FOR { <username> | USER | CURRENT_USER | PUBLIC }
    SERVER <servername>
    [ OPTIONS ( <option> '<value>' [, ... ] ) ]
```

See [CREATE USER MAPPING](sql_commands/CREATE_USER_MAPPING.html) for more information.

## <a id="ae1904779"></a>CREATE VIEW 

Defines a new view.

```
CREATE [OR REPLACE] [TEMP | TEMPORARY] [RECURSIVE] VIEW <name> [ ( <column_name> [, ...] ) ]
    [ WITH ( view_option_name [= view_option_value] [, ... ] ) ]
    AS <query>
    [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]
```

See [CREATE VIEW](sql_commands/CREATE_VIEW.html) for more information.

## <a id="ae1904789"></a>DEALLOCATE 

Deallocates a prepared statement.

```
DEALLOCATE [PREPARE] <name>
```

See [DEALLOCATE](sql_commands/DEALLOCATE.html) for more information.

## <a id="ae1904795"></a>DECLARE 

Defines a cursor.

```
DECLARE <name> [BINARY] [INSENSITIVE] [NO SCROLL] [PARALLEL RETRIEVE] CURSOR 
     [{WITH | WITHOUT} HOLD] 
     FOR <query> [FOR READ ONLY]
```

See [DECLARE](sql_commands/DECLARE.html) for more information.

## <a id="ae1904805"></a>DELETE 

Deletes rows from a table.

```
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
DELETE FROM [ONLY] <table> [[AS] <alias>]
      [USING <usinglist>]
      [WHERE <condition> | WHERE CURRENT OF <cursor_name>]
      [RETURNING * | <output_expression> [[AS] <output_name>] [, …]]
```

See [DELETE](sql_commands/DELETE.html) for more information.

## <a id="fixme"></a>DISCARD 

Discards the session state.

```
DISCARD { ALL | PLANS | TEMPORARY | TEMP }
```

See [DISCARD](sql_commands/DISCARD.html) for more information.

## <a id="ae1904815"></a>DROP AGGREGATE 

Removes an aggregate function.

```
DROP AGGREGATE [IF EXISTS] <name> ( <aggregate_signature> ) [CASCADE | RESTRICT]
```

See [DROP AGGREGATE](sql_commands/DROP_AGGREGATE.html) for more information.

## <a id="fixme"></a>DO 

Runs anonymous code block as a transient anonymous function.

```
DO [ LANGUAGE <lang_name> ] <code>
```

See [DO](sql_commands/DO.html) for more information.

## <a id="ae1904821"></a>DROP CAST 

Removes a cast.

```
DROP CAST [IF EXISTS] (<sourcetype> AS <targettype>) [CASCADE | RESTRICT]
```

See [DROP CAST](sql_commands/DROP_CAST.html) for more information.

## <a id="fixme"></a>DROP COLLATION 

Removes a previously defined collation.

```
DROP COLLATION [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

See [DROP COLLATION](sql_commands/DROP_COLLATION.html) for more information.

## <a id="section_s4r_fgy_xgb"></a>DROP CONVERSION 

Removes a conversion.

```
DROP CONVERSION [IF EXISTS] <name> [CASCADE | RESTRICT]
```

See [DROP CONVERSION](sql_commands/DROP_CONVERSION.html) for more information.

## <a id="ae1904833"></a>DROP DATABASE 

Removes a database.

```
DROP DATABASE [IF EXISTS] <name>
```

See [DROP DATABASE](sql_commands/DROP_DATABASE.html) for more information.

## <a id="ae1904839"></a>DROP DOMAIN 

Removes a domain.

```
DROP DOMAIN [IF EXISTS] <name> [, ...]  [CASCADE | RESTRICT]
```

See [DROP DOMAIN](sql_commands/DROP_DOMAIN.html) for more information.

## <a id="fixme"></a>DROP EXTENSION 

Removes an extension from a Greenplum database.

```
DROP EXTENSION [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

See [DROP EXTENSION](sql_commands/DROP_EXTENSION.html) for more information.

## <a id="ae1904845"></a>DROP EXTERNAL TABLE 

Removes an external table definition.

```
DROP EXTERNAL [WEB] TABLE [IF EXISTS] <name> [CASCADE | RESTRICT]
```

See [DROP EXTERNAL TABLE](sql_commands/DROP_EXTERNAL_TABLE.html) for more information.

## <a id="ae1904845w"></a>DROP FOREIGN DATA WRAPPER 

Removes a foreign-data wrapper.

```
DROP FOREIGN DATA WRAPPER [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

See [DROP FOREIGN DATA WRAPPER](sql_commands/DROP_FOREIGN_DATA_WRAPPER.html) for more information.

## <a id="ae1904845t"></a>DROP FOREIGN TABLE 

Removes a foreign table.

```
DROP FOREIGN TABLE [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

See [DROP FOREIGN TABLE](sql_commands/DROP_FOREIGN_TABLE.html) for more information.

## <a id="ae1904857"></a>DROP FUNCTION 

Removes a function.

```
DROP FUNCTION [IF EXISTS] name ( [ [argmode] [argname] argtype 
    [, ...] ] ) [CASCADE | RESTRICT]
```

See [DROP FUNCTION](sql_commands/DROP_FUNCTION.html) for more information.

## <a id="ae1904863"></a>DROP GROUP 

Removes a database role.

```
DROP GROUP [IF EXISTS] <name> [, ...]
```

See [DROP GROUP](sql_commands/DROP_GROUP.html) for more information.

## <a id="ae1904869"></a>DROP INDEX 

Removes an index.

```
DROP INDEX [ CONCURRENTLY ] [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

See [DROP INDEX](sql_commands/DROP_INDEX.html) for more information.

## <a id="ae1904875"></a>DROP LANGUAGE 

Removes a procedural language.

```
DROP [PROCEDURAL] LANGUAGE [IF EXISTS] <name> [CASCADE | RESTRICT]
```

See [DROP LANGUAGE](sql_commands/DROP_LANGUAGE.html) for more information.

## <a id="fixme"></a>DROP MATERIALIZED VIEW 

Removes a materialized view.

```
DROP MATERIALIZED VIEW [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

See [DROP MATERIALIZED VIEW](sql_commands/DROP_MATERIALIZED_VIEW.html) for more information.

## <a id="ae1904881"></a>DROP OPERATOR 

Removes an operator.

```
DROP OPERATOR [IF EXISTS] <name> ( {<lefttype> | NONE} , 
    {<righttype> | NONE} ) [CASCADE | RESTRICT]
```

See [DROP OPERATOR](sql_commands/DROP_OPERATOR.html) for more information.

## <a id="ae1904887"></a>DROP OPERATOR CLASS 

Removes an operator class.

```
DROP OPERATOR CLASS [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

See [DROP OPERATOR CLASS](sql_commands/DROP_OPERATOR_CLASS.html) for more information.

## <a id="fixme"></a>DROP OPERATOR FAMILY 

Removes an operator family.

```
DROP OPERATOR FAMILY [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

See [DROP OPERATOR FAMILY](sql_commands/DROP_OPERATOR_FAMILY.html) for more information.

## <a id="ae1904893"></a>DROP OWNED 

Removes database objects owned by a database role.

```
DROP OWNED BY <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP OWNED](sql_commands/DROP_OWNED.html) for more information.

## <a id="fixme"></a>DROP PROTOCOL 

Removes a external table data access protocol from a database.

```
DROP PROTOCOL [IF EXISTS] <name>
```

See [DROP PROTOCOL](sql_commands/DROP_PROTOCOL.html) for more information.

## <a id="ae19048993"></a>DROP RESOURCE GROUP 

Removes a resource group.

```
DROP RESOURCE GROUP <group_name>
```

See [DROP RESOURCE GROUP](sql_commands/DROP_RESOURCE_GROUP.html) for more information.

## <a id="ae1904899"></a>DROP RESOURCE QUEUE 

Removes a resource queue.

```
DROP RESOURCE QUEUE <queue_name>
```

See [DROP RESOURCE QUEUE](sql_commands/DROP_RESOURCE_QUEUE.html) for more information.

## <a id="ae1904905"></a>DROP ROLE 

Removes a database role.

```
DROP ROLE [IF EXISTS] <name> [, ...]
```

See [DROP ROLE](sql_commands/DROP_ROLE.html) for more information.

## <a id="ae1904911"></a>DROP RULE 

Removes a rewrite rule.

```
DROP RULE [IF EXISTS] <name> ON <table_name> [CASCADE | RESTRICT]
```

See [DROP RULE](sql_commands/DROP_RULE.html) for more information.

## <a id="ae1904917"></a>DROP SCHEMA 

Removes a schema.

```
DROP SCHEMA [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP SCHEMA](sql_commands/DROP_SCHEMA.html) for more information.

## <a id="ae1904923"></a>DROP SEQUENCE 

Removes a sequence.

```
DROP SEQUENCE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP SEQUENCE](sql_commands/DROP_SEQUENCE.html) for more information.

## <a id="ae1904923s"></a>DROP SERVER 

Removes a foreign server descriptor.

```
DROP SERVER [ IF EXISTS ] <servername> [ CASCADE | RESTRICT ]
```

See [DROP SERVER](sql_commands/DROP_SERVER.html) for more information.

## <a id="ae1904929"></a>DROP TABLE 

Removes a table.

```
DROP TABLE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP TABLE](sql_commands/DROP_TABLE.html) for more information.

## <a id="ae1904935"></a>DROP TABLESPACE 

Removes a tablespace.

```
DROP TABLESPACE [IF EXISTS] <tablespacename>
```

See [DROP TABLESPACE](sql_commands/DROP_TABLESPACE.html) for more information.

## <a id="fixme"></a>DROP TEXT SEARCH CONFIGURATION 

Removes a text search configuration.

```
DROP TEXT SEARCH CONFIGURATION [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

See [DROP TEXT SEARCH CONFIGURATION](sql_commands/DROP_TEXT_SEARCH_CONFIGURATION.html) for more information.

## <a id="fixme"></a>DROP TEXT SEARCH DICTIONARY 

Removes a text search dictionary.

```
DROP TEXT SEARCH DICTIONARY [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

See [DROP TEXT SEARCH DICTIONARY](sql_commands/DROP_TEXT_SEARCH_DICTIONARY.html) for more information.

## <a id="fixme"></a>DROP TEXT SEARCH PARSER 

Remove a text search parser.

```
DROP TEXT SEARCH PARSER [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

See [DROP TEXT SEARCH PARSER](sql_commands/DROP_TEXT_SEARCH_PARSER.html) for more information.

## <a id="fixme"></a>DROP TEXT SEARCH TEMPLATE 

Removes a text search template.

```
DROP TEXT SEARCH TEMPLATE [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

See [DROP TEXT SEARCH TEMPLATE](sql_commands/DROP_TEXT_SEARCH_TEMPLATE.html) for more information.

## <a id="ae1904947"></a>DROP TYPE 

Removes a data type.

```
DROP TYPE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP TYPE](sql_commands/DROP_TYPE.html) for more information.

## <a id="ae1904953"></a>DROP USER 

Removes a database role.

```
DROP USER [IF EXISTS] <name> [, ...]
```

See [DROP USER](sql_commands/DROP_USER.html) for more information.

## <a id="ae1904953u"></a>DROP USER MAPPING 

Removes a user mapping for a foreign server.

```
DROP USER MAPPING [ IF EXISTS ] { <username> | USER | CURRENT_USER | PUBLIC } 
    SERVER <servername>
```

See [DROP USER MAPPING](sql_commands/DROP_USER_MAPPING.html) for more information.

## <a id="ae1904959"></a>DROP VIEW 

Removes a view.

```
DROP VIEW [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP VIEW](sql_commands/DROP_VIEW.html) for more information.

## <a id="ae1904965"></a>END 

Commits the current transaction.

```
END [WORK | TRANSACTION]
```

See [END](sql_commands/END.html) for more information.

## <a id="ae1904971"></a>EXECUTE 

Runs a prepared SQL statement.

```
EXECUTE <name> [ (<parameter> [, ...] ) ]
```

See [EXECUTE](sql_commands/EXECUTE.html) for more information.

## <a id="ae1904977"></a>EXPLAIN 

Shows the query plan of a statement.

```
EXPLAIN [ ( <option> [, ...] ) ] <statement>
EXPLAIN [ANALYZE] [VERBOSE] <statement>
```

See [EXPLAIN](sql_commands/EXPLAIN.html) for more information.

## <a id="ae1904983"></a>FETCH 

Retrieves rows from a query using a cursor.

```
FETCH [ <forward_direction> { FROM | IN } ] <cursor_name>
```

See [FETCH](sql_commands/FETCH.html) for more information.

## <a id="ae1905011"></a>GRANT 

Defines access privileges.

```
GRANT { {SELECT | INSERT | UPDATE | DELETE | REFERENCES | 
TRIGGER | TRUNCATE } [, ...] | ALL [PRIVILEGES] }
    ON { [TABLE] <table_name> [, ...]
         | ALL TABLES IN SCHEMA <schema_name> [, ...] }
    TO { [ GROUP ] <role_name> | PUBLIC} [, ...] [ WITH GRANT OPTION ] 

GRANT { { SELECT | INSERT | UPDATE | REFERENCES } ( <column_name> [, ...] )
    [, ...] | ALL [ PRIVILEGES ] ( <column_name> [, ...] ) }
    ON [ TABLE ] <table_name> [, ...]
    TO { <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { {USAGE | SELECT | UPDATE} [, ...] | ALL [PRIVILEGES] }
    ON { SEQUENCE <sequence_name> [, ...]
         | ALL SEQUENCES IN SCHEMA <schema_name> [, ...] }
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ] 

GRANT { {CREATE | CONNECT | TEMPORARY | TEMP} [, ...] | ALL 
[PRIVILEGES] }
    ON DATABASE <database_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON DOMAIN <domain_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN DATA WRAPPER <fdw_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN SERVER <server_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { EXECUTE | ALL [PRIVILEGES] }
    ON { FUNCTION <function_name> ( [ [ <argmode> ] [ <argname> ] <argtype> [, ...] 
] ) [, ...]
        | ALL FUNCTIONS IN SCHEMA <schema_name> [, ...] }
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [PRIVILEGES] }
    ON LANGUAGE <lang_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { { CREATE | USAGE } [, ...] | ALL [PRIVILEGES] }
    ON SCHEMA <schema_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC}  [, ...] [ WITH GRANT OPTION ]

GRANT { CREATE | ALL [PRIVILEGES] }
    ON TABLESPACE <tablespace_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON TYPE <type_name> [, ...]
    TO { [ GROUP ] <role_name> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT <parent_role> [, ...] 
    TO <member_role> [, ...] [WITH ADMIN OPTION]

GRANT { SELECT | INSERT | ALL [PRIVILEGES] } 
    ON PROTOCOL <protocolname>
    TO <username>
```

See [GRANT](sql_commands/GRANT.html) for more information.

## <a id="ae1905069"></a>INSERT 

Creates new rows in a table.

```
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
INSERT INTO <table> [( <column> [, ...] )]
   {DEFAULT VALUES | VALUES ( {<expression> | DEFAULT} [, ...] ) [, ...] | <query>}
   [RETURNING * | <output_expression> [[AS] <output_name>] [, ...]]
```

See [INSERT](sql_commands/INSERT.html) for more information.

## <a id="ae1905077"></a>LOAD 

Loads or reloads a shared library file.

```
LOAD '<filename>'
```

See [LOAD](sql_commands/LOAD.html) for more information.

## <a id="ae1905083"></a>LOCK 

Locks a table.

```
LOCK [TABLE] [ONLY] name [ * ] [, ...] [IN <lockmode> MODE] [NOWAIT]
```

See [LOCK](sql_commands/LOCK.html) for more information.

## <a id="ae1905093"></a>MOVE 

Positions a cursor.

```
MOVE [ <forward_direction> [ FROM | IN ] ] <cursor_name>
```

See [MOVE](sql_commands/MOVE.html) for more information.

## <a id="ae1905121"></a>PREPARE 

Prepare a statement for execution.

```
PREPARE <name> [ (<datatype> [, ...] ) ] AS <statement>
```

See [PREPARE](sql_commands/PREPARE.html) for more information.

## <a id="ae1905127"></a>REASSIGN OWNED 

Changes the ownership of database objects owned by a database role.

```
REASSIGN OWNED BY <old_role> [, ...] TO <new_role>
```

See [REASSIGN OWNED](sql_commands/REASSIGN_OWNED.html) for more information.

## <a id="fixme"></a>REFRESH MATERIALIZED VIEW 

Replaces the contents of a materialized view.

```
REFRESH MATERIALIZED VIEW [ CONCURRENTLY ] <name>
    [ WITH [ NO ] DATA ]
```

See [REFRESH MATERIALIZED VIEW](sql_commands/REFRESH_MATERIALIZED_VIEW.html) for more information.

## <a id="ae1905133"></a>REINDEX 

Rebuilds indexes.

```
REINDEX {INDEX | TABLE | DATABASE | SYSTEM} <name>
```

See [REINDEX](sql_commands/REINDEX.html) for more information.

## <a id="ae1905139"></a>RELEASE SAVEPOINT 

Destroys a previously defined savepoint.

```
RELEASE [SAVEPOINT] <savepoint_name>
```

See [RELEASE SAVEPOINT](sql_commands/RELEASE_SAVEPOINT.html) for more information.

## <a id="ae1905145"></a>RESET 

Restores the value of a system configuration parameter to the default value.

```
RESET <configuration_parameter>

RESET ALL
```

See [RESET](sql_commands/RESET.html) for more information.

## <a id="retrieve"></a>RETRIEVE 

Retrieves rows from a query using a parallel retrieve cursor.

```
RETRIEVE { <count> | ALL } FROM ENDPOINT <endpoint_name>
```

See [RETRIEVE](sql_commands/RETRIEVE.html) for more information.

## <a id="ae1905153"></a>REVOKE 

Removes access privileges.

```
REVOKE [GRANT OPTION FOR] { {SELECT | INSERT | UPDATE | DELETE 
       | REFERENCES | TRIGGER | TRUNCATE } [, ...] | ALL [PRIVILEGES] }

       ON { [TABLE] <table_name> [, ...]
            | ALL TABLES IN SCHEMA schema_name [, ...] }
       FROM { [ GROUP ] <role_name> | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [ GRANT OPTION FOR ] { { SELECT | INSERT | UPDATE 
       | REFERENCES } ( <column_name> [, ...] )
       [, ...] | ALL [ PRIVILEGES ] ( <column_name> [, ...] ) }
       ON [ TABLE ] <table_name> [, ...]
       FROM { [ GROUP ]  <role_name> | PUBLIC } [, ...]
       [ CASCADE | RESTRICT ]

REVOKE [GRANT OPTION FOR] { {USAGE | SELECT | UPDATE} [,...] 
       | ALL [PRIVILEGES] }
       ON { SEQUENCE <sequence_name> [, ...]
            | ALL SEQUENCES IN SCHEMA schema_name [, ...] }
       FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] { {CREATE | CONNECT 
       | TEMPORARY | TEMP} [, ...] | ALL [PRIVILEGES] }
       ON DATABASE <database_name> [, ...]
       FROM { [ GROUP ] <role_name> | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [ GRANT OPTION FOR ]
       { USAGE | ALL [ PRIVILEGES ] }
       ON DOMAIN <domain_name> [, ...]
       FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
       [ CASCADE | RESTRICT ]


REVOKE [ GRANT OPTION FOR ]
       { USAGE | ALL [ PRIVILEGES ] }
       ON FOREIGN DATA WRAPPER <fdw_name> [, ...]
       FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
       [ CASCADE | RESTRICT ]

REVOKE [ GRANT OPTION FOR ]
       { USAGE | ALL [ PRIVILEGES ] }
       ON FOREIGN SERVER <server_name> [, ...]
       FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
       [ CASCADE | RESTRICT ]

REVOKE [GRANT OPTION FOR] {EXECUTE | ALL [PRIVILEGES]}
       ON { FUNCTION <funcname> ( [[<argmode>] [<argname>] <argtype>
                              [, ...]] ) [, ...]
            | ALL FUNCTIONS IN SCHEMA schema_name [, ...] }
       FROM { [ GROUP ] <role_name> | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] {USAGE | ALL [PRIVILEGES]}
       ON LANGUAGE <langname> [, ...]
       FROM { [ GROUP ]  <role_name> | PUBLIC} [, ...]
       [ CASCADE | RESTRICT ]

REVOKE [GRANT OPTION FOR] { {CREATE | USAGE} [, ...] 
       | ALL [PRIVILEGES] }
       ON SCHEMA <schema_name> [, ...]
       FROM { [ GROUP ] <role_name> | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] { CREATE | ALL [PRIVILEGES] }
       ON TABLESPACE <tablespacename> [, ...]
       FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
       [CASCADE | RESTRICT]

REVOKE [ GRANT OPTION FOR ]
       { USAGE | ALL [ PRIVILEGES ] }
       ON TYPE <type_name> [, ...]
       FROM { [ GROUP ] <role_name> | PUBLIC } [, ...]
       [ CASCADE | RESTRICT ] 

REVOKE [ADMIN OPTION FOR] <parent_role> [, ...] 
       FROM [ GROUP ] <member_role> [, ...]
       [CASCADE | RESTRICT]
```

See [REVOKE](sql_commands/REVOKE.html) for more information.

## <a id="ae1905229"></a>ROLLBACK 

Stops the current transaction.

```
ROLLBACK [WORK | TRANSACTION]
```

See [ROLLBACK](sql_commands/ROLLBACK.html) for more information.

## <a id="ae1905235"></a>ROLLBACK TO SAVEPOINT 

Rolls back the current transaction to a savepoint.

```
ROLLBACK [WORK | TRANSACTION] TO [SAVEPOINT] <savepoint_name>
```

See [ROLLBACK TO SAVEPOINT](sql_commands/ROLLBACK_TO_SAVEPOINT.html) for more information.

## <a id="ae1905241"></a>SAVEPOINT 

Defines a new savepoint within the current transaction.

```
SAVEPOINT <savepoint_name>
```

See [SAVEPOINT](sql_commands/SAVEPOINT.html) for more information.

## <a id="ae1905247"></a>SELECT 

Retrieves rows from a table or view.

```
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
SELECT [ALL | DISTINCT [ON (<expression> [, ...])]]
  * | <expression >[[AS] <output_name>] [, ...]
  [FROM <from_item> [, ...]]
  [WHERE <condition>]
  [GROUP BY <grouping_element> [, ...]]
  [HAVING <condition> [, ...]]
  [WINDOW <window_name> AS (<window_definition>) [, ...] ]
  [{UNION | INTERSECT | EXCEPT} [ALL | DISTINCT] <select>]
  [ORDER BY <expression> [ASC | DESC | USING <operator>] [NULLS {FIRST | LAST}] [, ...]]
  [LIMIT {<count> | ALL}]
  [OFFSET <start> [ ROW | ROWS ] ]
  [FETCH { FIRST | NEXT } [ <count> ] { ROW | ROWS } ONLY]
  [FOR2 {UPDATE | NO KEY UPDATE | SHARE | KEY SHARE} [OF <table_name> [, ...]] [NOWAIT] [...]]

TABLE { [ ONLY ] <table_name> [ * ] | <with_query_name> }

```

See [SELECT](sql_commands/SELECT.html) for more information.

## <a id="ae1905337"></a>SELECT INTO 

Defines a new table from the results of a query.

```
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
SELECT [ALL | DISTINCT [ON ( <expression> [, ...] )]]
    * | <expression> [AS <output_name>] [, ...]
    INTO [TEMPORARY | TEMP | UNLOGGED ] [TABLE] <new_table>
    [FROM <from_item> [, ...]]
    [WHERE <condition>]
    [GROUP BY <expression> [, ...]]
    [HAVING <condition> [, ...]]
    [{UNION | INTERSECT | EXCEPT} [ALL | DISTINCT ] <select>]
    [ORDER BY <expression> [ASC | DESC | USING <operator>] [NULLS {FIRST | LAST}] [, ...]]
    [LIMIT {<count> | ALL}]
    [OFFSET <start> [ ROW | ROWS ] ]
    [FETCH { FIRST | NEXT } [ <count> ] { ROW | ROWS } ONLY ]
    [FOR {UPDATE | SHARE} [OF <table_name> [, ...]] [NOWAIT] 
    [...]]
```

See [SELECT INTO](sql_commands/SELECT_INTO.html) for more information.

## <a id="ae1905365"></a>SET 

Changes the value of a Greenplum Database configuration parameter.

```
SET [SESSION | LOCAL] <configuration_parameter> {TO | =} <value> | 
    '<value>' | DEFAULT}

SET [SESSION | LOCAL] TIME ZONE {<timezone> | LOCAL | DEFAULT}
```

See [SET](sql_commands/SET.html) for more information.

## <a id="az1905373"></a>SET CONSTRAINTS 

Sets constraint check timing for the current transaction.

```
SET CONSTRAINTS { ALL | <name> [, ...] } { DEFERRED | IMMEDIATE }
```

See [SET CONSTRAINTS](sql_commands/SET_CONSTRAINTS.html) for more information.

## <a id="ae1905373"></a>SET ROLE 

Sets the current role identifier of the current session.

```
SET [SESSION | LOCAL] ROLE <rolename>

SET [SESSION | LOCAL] ROLE NONE

RESET ROLE
```

See [SET ROLE](sql_commands/SET_ROLE.html) for more information.

## <a id="ae1905383"></a>SET SESSION AUTHORIZATION 

Sets the session role identifier and the current role identifier of the current session.

```
SET [SESSION | LOCAL] SESSION AUTHORIZATION <rolename>

SET [SESSION | LOCAL] SESSION AUTHORIZATION DEFAULT

RESET SESSION AUTHORIZATION
```

See [SET SESSION AUTHORIZATION](sql_commands/SET_SESSION_AUTHORIZATION.html) for more information.

## <a id="ae1905393"></a>SET TRANSACTION 

Sets the characteristics of the current transaction.

```
SET TRANSACTION [<transaction_mode>] [READ ONLY | READ WRITE]

SET TRANSACTION SNAPSHOT <snapshot_id>

SET SESSION CHARACTERISTICS AS TRANSACTION <transaction_mode> 
     [READ ONLY | READ WRITE]
     [NOT] DEFERRABLE
```

See [SET TRANSACTION](sql_commands/SET_TRANSACTION.html) for more information.

## <a id="ae1905407"></a>SHOW 

Shows the value of a system configuration parameter.

```
SHOW <configuration_parameter>

SHOW ALL
```

See [SHOW](sql_commands/SHOW.html) for more information.

## <a id="ae1905415"></a>START TRANSACTION 

Starts a transaction block.

```
START TRANSACTION [<transaction_mode>] [READ WRITE | READ ONLY]
```

See [START TRANSACTION](sql_commands/START_TRANSACTION.html) for more information.

## <a id="ae1905421"></a>TRUNCATE 

Empties a table of all rows.

```
TRUNCATE [TABLE] [ONLY] <name> [ * ] [, ...] 
    [ RESTART IDENTITY | CONTINUE IDENTITY ] [CASCADE | RESTRICT]
```

See [TRUNCATE](sql_commands/TRUNCATE.html) for more information.

## <a id="ae1905427"></a>UPDATE 

Updates rows of a table.

```
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
UPDATE [ONLY] <table> [[AS] <alias>]
   SET {<column> = {<expression> | DEFAULT} |
   (<column> [, ...]) = ({<expression> | DEFAULT} [, ...])} [, ...]
   [FROM <fromlist>]
   [WHERE <condition >| WHERE CURRENT OF <cursor_name> ]
```

See [UPDATE](sql_commands/UPDATE.html) for more information.

## <a id="ae1905441"></a>VACUUM 

Garbage-collects and optionally analyzes a database.

```
VACUUM [({ FULL | FREEZE | VERBOSE | ANALYZE } [, ...])] [<table> [(<column> [, ...] )]]
        
VACUUM [FULL] [FREEZE] [VERBOSE] [<table>]

VACUUM [FULL] [FREEZE] [VERBOSE] ANALYZE
              [<table> [(<column> [, ...] )]]
```

See [VACUUM](sql_commands/VACUUM.html) for more information.

## <a id="ae1905451"></a>VALUES 

Computes a set of rows.

```
VALUES ( <expression> [, ...] ) [, ...]
   [ORDER BY <sort_expression> [ ASC | DESC | USING <operator> ] [, ...] ]
   [LIMIT { <count> | ALL } ] 
   [OFFSET <start> [ ROW | ROWS ] ]
   [FETCH { FIRST | NEXT } [<count> ] { ROW | ROWS } ONLY ]
```

See [VALUES](sql_commands/VALUES.html) for more information.

**Parent topic:** [SQL Commands](sql_commands/sql_ref.html)

