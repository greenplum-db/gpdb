# SQL Syntax Summary 

## ABORT 

Aborts the current transaction.

```
ABORT [WORK | TRANSACTION]
```

See [ABORT](sql_commands/ABORT.html) for more information.

## ALTER AGGREGATE 

Changes the definition of an aggregate function

```
ALTER AGGREGATE <name> ( <type> [ , ... ] ) RENAME TO <new_name>

ALTER AGGREGATE <name> ( <type> [ , ... ] ) OWNER TO <new_owner>

ALTER AGGREGATE <name> ( <type> [ , ... ] ) SET SCHEMA <new_schema>
```

See [ALTER AGGREGATE](sql_commands/ALTER_AGGREGATE.html) for more information.

## ALTER CONVERSION 

Changes the definition of a conversion.

```
ALTER CONVERSION <name> RENAME TO <newname>

ALTER CONVERSION <name> OWNER TO <newowner>
```

See [ALTER CONVERSION](sql_commands/ALTER_CONVERSION.html) for more information.

## ALTER DATABASE 

Changes the attributes of a database.

```
ALTER DATABASE <name> [ WITH CONNECTION LIMIT <connlimit> ]

ALTER DATABASE <name> SET <parameter> { TO | = } { <value> | DEFAULT }

ALTER DATABASE <name> RESET <parameter>

ALTER DATABASE <name> RENAME TO <newname>

ALTER DATABASE <name> OWNER TO <new_owner>
```

See [ALTER DATABASE](sql_commands/ALTER_DATABASE.html) for more information.

## ALTER DOMAIN 

Changes the definition of a domain.

```
ALTER DOMAIN <name> { SET DEFAULT <expression> | DROP DEFAULT }

ALTER DOMAIN <name> { SET | DROP } NOT NULL

ALTER DOMAIN <name> ADD <domain_constraint>

ALTER DOMAIN <name> DROP CONSTRAINT <constraint_name> [RESTRICT | CASCADE]

ALTER DOMAIN <name> OWNER TO <new_owner>

ALTER DOMAIN <name> SET SCHEMA <new_schema>
```

See [ALTER DOMAIN](sql_commands/ALTER_DOMAIN.html) for more information.

## ALTER EXTENSION 

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

* | [ <argmode> ] [ <argname> ] <argtype> [ , ... ] |
  [ [ <argmode> ] [ <argname> ] <argtype> [ , ... ] ] 
    ORDER BY [ <argmode> ] [ <argname> ] <argtype> [ , ... ]
```

See [ALTER EXTENSION](sql_commands/ALTER_EXTENSION.html) for more information.

## ALTER EXTERNAL TABLE 

Changes the definition of an external table.

``` {#d60e24}
ALTER EXTERNAL TABLE <name> <action> [, ... ]
```

where action is one of:

```
  ADD [COLUMN] <new_column> <type>
  DROP [COLUMN] <column> [RESTRICT|CASCADE]
  ALTER [COLUMN] <column> TYPE <type> [USING <expression>]
  OWNER TO <new_owner>
```

See [ALTER EXTERNAL TABLE](sql_commands/ALTER_EXTERNAL_TABLE.html) for more information.

## ALTER FILESPACE 

Changes the definition of a filespace.

```
ALTER FILESPACE <name> RENAME TO <newname>

ALTER FILESPACE <name> OWNER TO <newowner>
```

See [ALTER FILESPACE](sql_commands/ALTER_FILESPACE.html) for more information.

## ALTER FUNCTION 

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

## ALTER GROUP 

Changes a role name or membership.

```
ALTER GROUP <groupname> ADD USER <username> [, ... ]

ALTER GROUP <groupname> DROP USER <username> [, ... ]

ALTER GROUP <groupname> RENAME TO <newname>
```

See [ALTER GROUP](sql_commands/ALTER_GROUP.html) for more information.

## ALTER INDEX 

Changes the definition of an index.

```
ALTER INDEX <name> RENAME TO <new_name>

ALTER INDEX <name> SET TABLESPACE <tablespace_name>

ALTER INDEX <name> SET ( FILLFACTOR = <value> )

ALTER INDEX <name> RESET ( FILLFACTOR )
```

See [ALTER INDEX](sql_commands/ALTER_INDEX.html) for more information.

## ALTER LANGUAGE 

Changes the name of a procedural language.

```
ALTER LANGUAGE <name> RENAME TO <newname>
ALTER LANGUAGE <name> OWNER TO <new_owner>
```

See [ALTER LANGUAGE](sql_commands/ALTER_LANGUAGE.html) for more information.

## ALTER OPERATOR 

Changes the definition of an operator.

```
ALTER OPERATOR <name> ( {<lefttype> | NONE} , {<righttype> | NONE} ) 
   OWNER TO <newowner>
```

See [ALTER OPERATOR](sql_commands/ALTER_OPERATOR.html) for more information.

## ALTER OPERATOR CLASS 

Changes the definition of an operator class.

```
ALTER OPERATOR CLASS <name> USING <index_method> RENAME TO <newname>

ALTER OPERATOR CLASS <name> USING <index_method> OWNER TO <newowner>
```

See [ALTER OPERATOR CLASS](sql_commands/ALTER_OPERATOR_CLASS.html) for more information.

## ALTER OPERATOR FAMILY 

Changes the definition of an operator family.

```
ALTER OPERATOR FAMILY <name> USING <index_method> ADD
  {  OPERATOR <strategy_number> <operator_name> ( <op_type>, <op_type> ) [ RECHECK ]
    | FUNCTION <support_number> [ ( <op_type> [ , <op_type> ] ) ] <funcname> ( <argument_type> [, ...] )
  } [, ... ]
ALTER OPERATOR FAMILY <name> USING <index_method> DROP
  {  OPERATOR s<trategy_number> ( <op_type>, <op_type> ) 
    | FUNCTION <support_number> [ ( <op_type> [ , <op_type> ] ) 
  } [, ... ]

ALTER OPERATOR FAMILY <name> USING <index_method> RENAME TO <newname>

ALTER OPERATOR FAMILY <name> USING <index_method> OWNER TO <newowner>
```

See [ALTER OPERATOR FAMILY](sql_commands/ALTER_OPERATOR_FAMILY.html) for more information.

## ALTER PROTOCOL 

Changes the definition of a protocol.

```
ALTER PROTOCOL <name> RENAME TO <newname>

ALTER PROTOCOL <name> OWNER TO <newowner>
```

See [ALTER PROTOCOL](sql_commands/ALTER_PROTOCOL.html) for more information.

## ALTER RESOURCE GROUP 

Changes the limits of a resource group.

```
ALTER RESOURCE GROUP <name> SET <group_attribute> <value>
```

See [ALTER RESOURCE GROUP](sql_commands/ALTER_RESOURCE_GROUP.html) for more information.

## ALTER RESOURCE QUEUE 

Changes the limits of a resource queue.

```
ALTER RESOURCE QUEUE <name> WITH ( <queue_attribute>=<value> [, ... ] ) 
```

See [ALTER RESOURCE QUEUE](sql_commands/ALTER_RESOURCE_QUEUE.html) for more information.

## ALTER ROLE 

Changes a database role \(user or group\).

```
ALTER ROLE <name> RENAME TO <newname>

ALTER ROLE <name> SET <config_parameter> {TO | =} {<value> | DEFAULT}

ALTER ROLE <name> RESET <config_parameter>

ALTER ROLE <name> RESOURCE QUEUE {<queue_name> | NONE}

ALTER ROLE <name> RESOURCE GROUP {<group_name> | NONE}

ALTER ROLE <name> [ [WITH] <option> [ ... ] ]
```

See [ALTER ROLE](sql_commands/ALTER_ROLE.html) for more information.

## ALTER SCHEMA 

Changes the definition of a schema.

```
ALTER SCHEMA <name> RENAME TO <newname>

ALTER SCHEMA <name> OWNER TO <newowner>
```

See [ALTER SCHEMA](sql_commands/ALTER_SCHEMA.html) for more information.

## ALTER SEQUENCE 

Changes the definition of a sequence generator.

```
ALTER SEQUENCE <name> [INCREMENT [ BY ] <increment>] 
     [MINVALUE <minvalue> | NO MINVALUE] 
     [MAXVALUE <maxvalue> | NO MAXVALUE] 
     [RESTART [ WITH ] <start>] 
     [CACHE <cache>] [[ NO ] CYCLE] 
     [OWNED BY {<table.column> | NONE}]

ALTER SEQUENCE <name> RENAME TO new\_name

ALTER SEQUENCE <name> SET SCHEMA <new_schema>
```

See [ALTER SEQUENCE](sql_commands/ALTER_SEQUENCE.html) for more information.

## ALTER TABLE 

Changes the definition of a table.

```
ALTER TABLE [ONLY] <name> RENAME [COLUMN] <column> TO <new_column>

ALTER TABLE <name> RENAME TO <new_name>

ALTER TABLE <name> SET SCHEMA <new_schema>

ALTER TABLE [ONLY] <name> SET 
     DISTRIBUTED BY (<column>, [ ... ] ) 
   | DISTRIBUTED RANDOMLY 
   | WITH (REORGANIZE=true|false)
 
ALTER TABLE [ONLY] <name> <action> [, ... ]

ALTER TABLE <name>
   [ ALTER PARTITION { <partition_name> | FOR (RANK(<number>)) 
   | FOR (<value>) } [...] ] <partition_action>

where <action> is one of:
                        
  ADD [COLUMN] <column_name data_type> [ DEFAULT <default_expr> ]
      [<column_constraint> [ ... ]]
      [ ENCODING ( <storage_directive> [,...] ) ]
  DROP [COLUMN] <column> [RESTRICT | CASCADE]
  ALTER [COLUMN] <column> TYPE <type> [USING <expression>]
  ALTER [COLUMN] <column> SET DEFAULT <expression>
  ALTER [COLUMN] <column> DROP DEFAULT
  ALTER [COLUMN] <column> { SET | DROP } NOT NULL
  ALTER [COLUMN] <column> SET STATISTICS <integer>
  ADD <table_constraint>
  DROP CONSTRAINT <constraint_name> [RESTRICT | CASCADE]
  DISABLE TRIGGER [<trigger_name> | ALL | USER]
  ENABLE TRIGGER [<trigger_name> | ALL | USER]
  CLUSTER ON <index_name>
  SET WITHOUT CLUSTER
  SET WITHOUT OIDS
  SET (FILLFACTOR = <value>)
  RESET (FILLFACTOR)
  INHERIT <parent_table>
  NO INHERIT <parent_table>
  OWNER TO <new_owner>
  SET TABLESPACE <new_tablespace>
```

See [ALTER TABLE](sql_commands/ALTER_TABLE.html) for more information.

## ALTER TABLESPACE 

Changes the definition of a tablespace.

```
ALTER TABLESPACE <name> RENAME TO <newname>

ALTER TABLESPACE <name> OWNER TO <newowner>
```

See [ALTER TABLESPACE](sql_commands/ALTER_TABLESPACE.html) for more information.

## ALTER TYPE 

Changes the definition of a data type.

```
ALTER TYPE <name>
   OWNER TO <new_owner> | SET SCHEMA <new_schema>
```

See [ALTER TYPE](sql_commands/ALTER_TYPE.html) for more information.

## ALTER USER 

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

## ALTER VIEW 

Changes the definition of a view.

```
ALTER VIEW <name> RENAME TO <newname>

```

See [ALTER VIEW](sql_commands/ALTER_VIEW.html) for more information.

## ANALYZE 

Collects statistics about a database.

```
ANALYZE [VERBOSE] [<table> [ (<column> [, ...] ) ]]

ANALYZE [VERBOSE] {<root_partition_table_name>|<leaf_partition_table_name>} [ (<column> [, ...] )] 

ANALYZE [VERBOSE] ROOTPARTITION {ALL | <root_partition_table_name> [ (<column> [, ...] )]}
```

See [ANALYZE](sql_commands/ANALYZE.html) for more information.

## BEGIN 

Starts a transaction block.

```
BEGIN [WORK | TRANSACTION] [<transaction_mode>]
      [READ ONLY | READ WRITE]
```

See [BEGIN](sql_commands/BEGIN.html) for more information.

## CHECKPOINT 

Forces a transaction log checkpoint.

```
CHECKPOINT
```

See [CHECKPOINT](sql_commands/CHECKPOINT.html) for more information.

## CLOSE 

Closes a cursor.

```
CLOSE <cursor_name>
```

See [CLOSE](sql_commands/CLOSE.html) for more information.

## CLUSTER 

Physically reorders a heap storage table on disk according to an index. Not a recommended operation in Greenplum Database.

```
CLUSTER <indexname> ON <tablename>

CLUSTER <tablename>

CLUSTER
```

See [CLUSTER](sql_commands/CLUSTER.html) for more information.

## COMMENT 

Defines or change the comment of an object.

```
COMMENT ON
{ TABLE <object_name> |
  COLUMN <table_name.column_name> |
  AGGREGATE <agg_name> (<agg_type> [, ...]) |
  CAST (<sourcetype> AS <targettype>) |
  CONSTRAINT <constraint_name> ON <table_name> |
  CONVERSION <object_name> |
  DATABASE <object_name> |
  DOMAIN <object_name> |
  FILESPACE <object_name> |
  FUNCTION <func_name> ([[<argmode>] [<argname>] <argtype> [, ...]]) |
  INDEX <object_name> |
  LARGE OBJECT <large_object_oid> |
  OPERATOR <op> (<leftoperand_type>, <rightoperand_type>) |
  OPERATOR CLASS <object_name> USING <index_method> |
  [PROCEDURAL] LANGUAGE <object_name> |
  RESOURCE QUEUE <object_name> |
  ROLE <object_name> |
  RULE <rule_name> ON <table_name> |
  SCHEMA <object_name> |
  SEQUENCE <object_name> |
  TABLESPACE <object_name> |
  TRIGGER <trigger_name> ON <table_name> |
  TYPE <object_name> |
  VIEW <object_name> } 
IS '<text>'
```

See [COMMENT](sql_commands/COMMENT.html) for more information.

## COMMIT 

Commits the current transaction.

```
COMMIT [WORK | TRANSACTION]
```

See [COMMIT](sql_commands/COMMIT.html) for more information.

## COPY 

Copies data between a file and a table.

```
COPY <table> [(<column> [, ...])] FROM {'<file>' | PROGRAM '<command>' | STDIN}
     [ [WITH]  
       [ON SEGMENT]
       [BINARY]
       [OIDS]
       [HEADER]
       [DELIMITER [ AS ] '<delimiter>']
       [NULL [ AS ] '<null string>']
       [ESCAPE [ AS ] '<escape>' | 'OFF']
       [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
       [CSV [QUOTE [ AS ] '<quote>'] 
            [FORCE NOT NULL <column> [, ...]]
       [FILL MISSING FIELDS]
       [[LOG ERRORS]  
       SEGMENT REJECT LIMIT <count> [ROWS | PERCENT] ]

COPY {table [(<column> [, ...])] | (<query>)} TO {'<file>' | PROGRAM '<command>' | STDOUT}
      [ [WITH] 
        [ON SEGMENT]
        [BINARY]
        [OIDS]
        [HEADER]
        [DELIMITER [ AS ] 'delimiter']
        [NULL [ AS ] 'null string']
        [ESCAPE [ AS ] '<escape>' | 'OFF']
        [CSV [QUOTE [ AS ] 'quote'] 
             [FORCE QUOTE column [, ...]] ]
      [IGNORE EXTERNAL PARTITIONS ]
```

See [COPY](sql_commands/COPY.html) for more information.

## CREATE AGGREGATE 

Defines a new aggregate function.

```
CREATE [ORDERED] AGGREGATE <name> (<input_data_type> [ , ... ]) 
      ( SFUNC = <sfunc>,
        STYPE = <state_data_type>
        [, PREFUNC = <prefunc>]
        [, FINALFUNC = <ffunc>]
        [, INITCOND = <initial_condition>]
        [, SORTOP = <sort_operator>] )
```

See [CREATE AGGREGATE](sql_commands/CREATE_AGGREGATE.html) for more information.

## CREATE CAST 

Defines a new cast.

```
CREATE CAST (<sourcetype> AS <targettype>) 
       WITH FUNCTION <funcname> (<argtypes>) 
       [AS ASSIGNMENT | AS IMPLICIT]

CREATE CAST (<sourcetype> AS <targettype>) WITHOUT FUNCTION 
       [AS ASSIGNMENT | AS IMPLICIT]
```

See [CREATE CAST](sql_commands/CREATE_CAST.html) for more information.

## CREATE CONVERSION 

Defines a new encoding conversion.

```
CREATE [DEFAULT] CONVERSION <name> FOR <source_encoding> TO 
     <dest_encoding> FROM <funcname>
```

See [CREATE CONVERSION](sql_commands/CREATE_CONVERSION.html) for more information.

## CREATE DATABASE 

Creates a new database.

```
CREATE DATABASE name [ [WITH] [OWNER [=] <dbowner>]
                     [TEMPLATE [=] <template>]
                     [ENCODING [=] <encoding>]
                     [TABLESPACE [=] <tablespace>]
                     [CONNECTION LIMIT [=] connlimit ] ]
```

See [CREATE DATABASE](sql_commands/CREATE_DATABASE.html) for more information.

## CREATE DOMAIN 

Defines a new domain.

```
CREATE DOMAIN <name> [AS] <data_type> [DEFAULT <expression>] 
       [CONSTRAINT <constraint_name>
       | NOT NULL | NULL 
       | CHECK (<expression>) [...]]
```

See [CREATE DOMAIN](sql_commands/CREATE_DOMAIN.html) for more information.

## CREATE EXTENSION 

Registers an extension in a Greenplum database.

```
CREATE EXTENSION [ IF NOT EXISTS ] <extension_name>
  [ WITH ] [ SCHEMA <schema_name> ]
           [ VERSION <version> ]
           [ FROM <old_version> ]
           [ CASCADE ]
```

See [CREATE EXTENSION](sql_commands/CREATE_EXTENSION.html) for more information.

## CREATE EXTERNAL TABLE 

Defines a new external table.

```
CREATE [READABLE] EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>     
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION ('file://<seghost>[:<port>]/<path>/<file>' [, ...])
       | ('gpfdist://<filehost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
           [, ...]
       | ('gpfdists://<filehost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
           [, ...])
       | ('gphdfs://<hdfs_host>[:port]/<path>/<file>')
       | ('pxf://<path-to-data>?<PROFILE>[&<custom-option>=<value>[...]]'))
       | ('s3://<S3_endpoint>[:<port>]/<bucket_name>/[<S3_prefix>] [region=<S3-region>] [config=<config_file>]')
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
          | 'AVRO' 
          | 'PARQUET'
          | 'CUSTOM' (Formatter=<<formatter_specifications>>)
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
     [ ENCODING '<encoding>' ]
     [ [LOG ERRORS [PERSISTENTLY]] SEGMENT REJECT LIMIT <count>
       [ROWS | PERCENT] ]

CREATE WRITABLE EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION('gpfdist://<outputhost>[:<port>]/<filename>[#transform=<trans_name>]'
          [, ...])
      | ('gpfdists://<outputhost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
          [, ...])
      | ('gphdfs://<hdfs_host>[:port]/<path>')
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
           | 'AVRO' 
           | 'PARQUET'

           | 'CUSTOM' (Formatter=<<formatter specifications>>)
    [ ENCODING '<write_encoding>' ]
    [ DISTRIBUTED BY (<column>, [ ... ] ) | DISTRIBUTED RANDOMLY ]

CREATE WRITABLE EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION('s3://<S3_endpoint>[:<port>]/<bucket_name>/[<S3_prefix>] [region=<S3-region>] [config=<config_file>]')
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
    [ ENCODING '<write_encoding>' ]
    [ DISTRIBUTED BY (<column>, [ ... ] ) | DISTRIBUTED RANDOMLY ]
```

See [CREATE EXTERNAL TABLE](sql_commands/CREATE_EXTERNAL_TABLE.html) for more information.

## CREATE FUNCTION 

Defines a new function.

```
CREATE [OR REPLACE] FUNCTION <name>    
    ( [ [<argmode>] [<argname>] <argtype> [ { DEFAULT | = } <defexpr> ] [, ...] ] )
      [ RETURNS { [ SETOF ] rettype 
        | TABLE ([{ argname argtype | LIKE other table }
          [, ...]])
        } ]
    { LANGUAGE <langname>
    | IMMUTABLE | STABLE | VOLATILE
    | CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT
    | NO SQL | CONTAINS SQL | READS SQL DATA | MODIFIES SQL
    | [EXTERNAL] SECURITY INVOKER | [EXTERNAL] SECURITY DEFINER
    | COST <execution_cost>
    | SET <configuration_parameter> { TO <value> | = <value> | FROM CURRENT }
    | AS '<definition>'
    | AS '<obj_file>', '<link_symbol>' } ...
    [ WITH ({ DESCRIBE = describe_function
           } [, ...] ) ]
```

See [CREATE FUNCTION](sql_commands/CREATE_FUNCTION.html) for more information.

## CREATE GROUP 

Defines a new database role.

```
CREATE GROUP <name> [[WITH] <option> [ ... ]]
```

See [CREATE GROUP](sql_commands/CREATE_GROUP.html) for more information.

## CREATE INDEX 

Defines a new index.

```
CREATE [UNIQUE] INDEX <name> ON <table>
       [USING btree|bitmap|gist]
       ( {<column> | (<expression>)} [<opclass>] [, ...] )
       [ WITH ( FILLFACTOR = <value> ) ]
       [TABLESPACE <tablespace>]
       [WHERE <predicate>]
```

See [CREATE INDEX](sql_commands/CREATE_INDEX.html) for more information.

## CREATE LANGUAGE 

Defines a new procedural language.

```
CREATE [PROCEDURAL] LANGUAGE <name>

CREATE [TRUSTED] [PROCEDURAL] LANGUAGE <name>
       HANDLER <call_handler> [ INLINE <inline_handler> ] [VALIDATOR <valfunction>]
```

See [CREATE LANGUAGE](sql_commands/CREATE_LANGUAGE.html) for more information.

## CREATE OPERATOR 

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

## CREATE OPERATOR CLASS 

Defines a new operator class.

```
CREATE OPERATOR CLASS <name> [DEFAULT] FOR TYPE <data_type>  
  USING <index_method> AS 
  { 
  OPERATOR <strategy_number> <op_name> [(<op_type>, <op_type>)] [RECHECK]
  | FUNCTION <support_number> <funcname> (<argument_type> [, ...] )
  | STORAGE <storage_type>
  } [, ... ]
```

See [CREATE OPERATOR CLASS](sql_commands/CREATE_OPERATOR_CLASS.html) for more information.

## CREATE OPERATOR FAMILY 

Defines a new operator family.

```
CREATE OPERATOR FAMILY <name>  USING <index_method>  
```

See [CREATE OPERATOR FAMILY](sql_commands/CREATE_OPERATOR_FAMILY.html) for more information.

## CREATE PROTOCOL 

Registers a custom data access protocol that can be specified when defining a Greenplum Database external table.

```
CREATE [TRUSTED] PROTOCOL <name> (
   [readfunc='<read_call_handler>'] [, writefunc='<write_call_handler>']
   [, validatorfunc='<validate_handler>' ])
```

See [CREATE PROTOCOL](sql_commands/CREATE_PROTOCOL.html) for more information.

## CREATE RESOURCE GROUP 

Defines a new resource group.

```
CREATE RESOURCE GROUP <name> WITH (<group_attribute>=<value> [, ... ])
```

See [CREATE RESOURCE GROUP](sql_commands/CREATE_RESOURCE_GROUP.html) for more information.

## CREATE RESOURCE QUEUE 

Defines a new resource queue.

```
CREATE RESOURCE QUEUE <name> WITH (<queue_attribute>=<value> [, ... ])
```

See [CREATE RESOURCE QUEUE](sql_commands/CREATE_RESOURCE_QUEUE.html) for more information.

## CREATE ROLE 

Defines a new database role \(user or group\).

```
CREATE ROLE <name> [[WITH] <option> [ ... ]]
```

See [CREATE ROLE](sql_commands/CREATE_ROLE.html) for more information.

## CREATE RULE 

Defines a new rewrite rule.

```
CREATE [OR REPLACE] RULE <name> AS ON <event>
  TO <table> [WHERE <condition>] 
  DO [ALSO | INSTEAD] { NOTHING | <command> | (<command>; <command> 
  ...) }
```

See [CREATE RULE](sql_commands/CREATE_RULE.html) for more information.

## CREATE SCHEMA 

Defines a new schema.

```
CREATE SCHEMA <schema_name> [AUTHORIZATION <username>] 
   [<schema_element> [ ... ]]

CREATE SCHEMA AUTHORIZATION <rolename> [<schema_element> [ ... ]]
```

See [CREATE SCHEMA](sql_commands/CREATE_SCHEMA.html) for more information.

## CREATE SEQUENCE 

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

## CREATE TABLE 

Defines a new table.

```
CREATE [[GLOBAL | LOCAL] {TEMPORARY | TEMP}] TABLE <table_name> ( 
[ { <column_name> <data_type> [ DEFAULT <default_expr> ] 
   [<column_constraint> [ ... ]
[ ENCODING ( <storage_directive> [,...] ) ]
] 
   | <table_constraint>
   | LIKE <other_table> [{INCLUDING | EXCLUDING} 
                      {DEFAULTS | CONSTRAINTS}] ...}
   [, ... ] ]
   )
   [ INHERITS ( <parent_table> [, ... ] ) ]
   [ WITH ( <storage_parameter>=<value> [, ... ] )
   [ ON COMMIT {PRESERVE ROWS | DELETE ROWS | DROP} ]
   [ TABLESPACE <tablespace> ]
   [ DISTRIBUTED BY (<column>, [ ... ] ) | DISTRIBUTED RANDOMLY ]
   [ PARTITION BY <partition_type> (<column>)
       [ SUBPARTITION BY <partition_type> (<column>) ] 
          [ SUBPARTITION TEMPLATE ( <template_spec >) ]
       [...]
    ( <partition_spec> ) 
        | [ SUBPARTITION BY <partition_type> (<column>) ]
          [...]
    ( <partition_spec>
      [ ( <subpartition_spec>
           [(...)] 
         ) ] 
    )
```

See [CREATE TABLE](sql_commands/CREATE_TABLE.html) for more information.

## CREATE TABLE AS 

Defines a new table from the results of a query.

```
CREATE [ [GLOBAL | LOCAL] {TEMPORARY | TEMP} ] TABLE <table_name>
   [(<column_name> [, ...] )]
   [ WITH ( <storage_parameter>=<value> [, ... ] ) ]
   [ON COMMIT {PRESERVE ROWS | DELETE ROWS | DROP}]
   [TABLESPACE <tablespace>]
   AS <query>
   [DISTRIBUTED BY (<column>, [ ... ] ) | DISTRIBUTED RANDOMLY]
```

See [CREATE TABLE AS](sql_commands/CREATE_TABLE_AS.html) for more information.

## CREATE TABLESPACE 

Defines a new tablespace.

```
CREATE TABLESPACE <tablespace_name> [OWNER <username>] 
       FILESPACE <filespace_name>
```

See [CREATE TABLESPACE](sql_commands/CREATE_TABLESPACE.html) for more information.

## CREATE TYPE 

Defines a new data type.

```
CREATE TYPE <name> AS ( <attribute_name> <data_type> [, ... ] )

CREATE TYPE <name> AS ENUM ( '<label>' [, ... ] )

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
    [, DEFAULT = <default>]
    [, ELEMENT = <element>]
    [, DELIMITER = <delimiter>] )

CREATE TYPE <name>
```

See [CREATE TYPE](sql_commands/CREATE_TYPE.html) for more information.

## CREATE USER 

Defines a new database role with the `LOGIN` privilege by default.

```
CREATE USER <name> [[WITH] <option> [ ... ]]
```

See [CREATE USER](sql_commands/CREATE_USER.html) for more information.

## CREATE VIEW 

Defines a new view.

```
CREATE [OR REPLACE] [TEMP | TEMPORARY] VIEW <name>
       [ ( <column_name> [, ...] ) ]
       AS <query>
```

See [CREATE VIEW](sql_commands/CREATE_VIEW.html) for more information.

## DEALLOCATE 

Deallocates a prepared statement.

```
DEALLOCATE [PREPARE] <name>
```

See [DEALLOCATE](sql_commands/DEALLOCATE.html) for more information.

## DECLARE 

Defines a cursor.

```
DECLARE <name> [BINARY] [INSENSITIVE] [NO SCROLL] CURSOR 
     [{WITH | WITHOUT} HOLD] 
     FOR <query> [FOR READ ONLY]
```

See [DECLARE](sql_commands/DECLARE.html) for more information.

## DELETE 

Deletes rows from a table.

```
DELETE FROM [ONLY] <table> [[AS] <alias>]
      [USING <usinglist>]
      [WHERE <condition> | WHERE CURRENT OF <cursor_name> ]
```

See [DELETE](sql_commands/DELETE.html) for more information.

## DISCARD 

Discards the session state.

```
DISCARD { ALL | PLANS | TEMPORARY | TEMP }
```

See [DISCARD](sql_commands/DISCARD.html) for more information.

## DROP AGGREGATE 

Removes an aggregate function.

```
DROP AGGREGATE [IF EXISTS] <name> ( <type> [, ...] ) [CASCADE | RESTRICT]
```

See [DROP AGGREGATE](sql_commands/DROP_AGGREGATE.html) for more information.

## DO 

Executes an anonymous code block as a transient anonymous function.

```
DO [ LANGUAGE <lang_name> ] <code>
```

See [DO](sql_commands/DO.html) for more information.

## DROP CAST 

Removes a cast.

```
DROP CAST [IF EXISTS] (<sourcetype> AS <targettype>) [CASCADE | RESTRICT]
```

See [DROP CAST](sql_commands/DROP_CAST.html) for more information.

## DROP CONVERSION 

Removes a conversion.

```
DROP CONVERSION [IF EXISTS] <name> [CASCADE | RESTRICT]
```

See [DROP CONVERSION](sql_commands/DROP_CONVERSION.html) for more information.

## DROP DATABASE 

Removes a database.

```
DROP DATABASE [IF EXISTS] <name>
```

See [DROP DATABASE](sql_commands/DROP_DATABASE.html) for more information.

## DROP DOMAIN 

Removes a domain.

```
DROP DOMAIN [IF EXISTS] <name> [, ...]  [CASCADE | RESTRICT]
```

See [DROP DOMAIN](sql_commands/DROP_DOMAIN.html) for more information.

## DROP EXTENSION 

Removes an extension from a Greenplum database.

```
DROP EXTENSION [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

See [DROP EXTENSION](sql_commands/DROP_EXTENSION.html) for more information.

## DROP EXTERNAL TABLE 

Removes an external table definition.

```
DROP EXTERNAL [WEB] TABLE [IF EXISTS] <name> [CASCADE | RESTRICT]
```

See [DROP EXTERNAL TABLE](sql_commands/DROP_EXTERNAL_TABLE.html) for more information.

## DROP FILESPACE 

Removes a filespace.

```
DROP FILESPACE [IF EXISTS] <filespacename>
```

See [DROP FILESPACE](sql_commands/DROP_FILESPACE.html) for more information.

## DROP FUNCTION 

Removes a function.

```
DROP FUNCTION [IF EXISTS] name ( [ [argmode] [argname] argtype 
    [, ...] ] ) [CASCADE | RESTRICT]
```

See [DROP FUNCTION](sql_commands/DROP_FUNCTION.html) for more information.

## DROP GROUP 

Removes a database role.

```
DROP GROUP [IF EXISTS] <name> [, ...]
```

See [DROP GROUP](sql_commands/DROP_GROUP.html) for more information.

## DROP INDEX 

Removes an index.

```
DROP INDEX [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP INDEX](sql_commands/DROP_INDEX.html) for more information.

## DROP LANGUAGE 

Removes a procedural language.

```
DROP [PROCEDURAL] LANGUAGE [IF EXISTS] <name> [CASCADE | RESTRICT]
```

See [DROP LANGUAGE](sql_commands/DROP_LANGUAGE.html) for more information.

## DROP OPERATOR 

Removes an operator.

```
DROP OPERATOR [IF EXISTS] <name> ( {<lefttype> | NONE} , 
    {<righttype> | NONE} ) [CASCADE | RESTRICT]
```

See [DROP OPERATOR](sql_commands/DROP_OPERATOR.html) for more information.

## DROP OPERATOR CLASS 

Removes an operator class.

```
DROP OPERATOR CLASS [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

See [DROP OPERATOR CLASS](sql_commands/DROP_OPERATOR_CLASS.html) for more information.

## DROP OPERATOR FAMILY 

Removes an operator family.

```
DROP OPERATOR FAMILY [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

See [DROP OPERATOR FAMILY](sql_commands/DROP_OPERATOR_FAMILY.html) for more information.

## DROP OWNED 

Removes database objects owned by a database role.

```
DROP OWNED BY <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP OWNED](sql_commands/DROP_OWNED.html) for more information.

## DROP PROTOCOL 

Removes a external table data access protocol from a database.

```
DROP PROTOCOL [IF EXISTS] <name>
```

See [DROP PROTOCOL](sql_commands/DROP_PROTOCOL.html) for more information.

## DROP RESOURCE GROUP 

Removes a resource group.

```
DROP RESOURCE GROUP <group_name>
```

See [DROP RESOURCE GROUP](sql_commands/DROP_RESOURCE_GROUP.html) for more information.

## DROP RESOURCE QUEUE 

Removes a resource queue.

```
DROP RESOURCE QUEUE <queue_name>
```

See [DROP RESOURCE QUEUE](sql_commands/DROP_RESOURCE_QUEUE.html) for more information.

## DROP ROLE 

Removes a database role.

```
DROP ROLE [IF EXISTS] <name> [, ...]
```

See [DROP ROLE](sql_commands/DROP_ROLE.html) for more information.

## DROP RULE 

Removes a rewrite rule.

```
DROP RULE [IF EXISTS] <name> ON <relation> [CASCADE | RESTRICT]
```

See [DROP RULE](sql_commands/DROP_RULE.html) for more information.

## DROP SCHEMA 

Removes a schema.

```
DROP SCHEMA [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP SCHEMA](sql_commands/DROP_SCHEMA.html) for more information.

## DROP SEQUENCE 

Removes a sequence.

```
DROP SEQUENCE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP SEQUENCE](sql_commands/DROP_SEQUENCE.html) for more information.

## DROP TABLE 

Removes a table.

```
DROP TABLE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP TABLE](sql_commands/DROP_TABLE.html) for more information.

## DROP TABLESPACE 

Removes a tablespace.

```
DROP TABLESPACE [IF EXISTS] <tablespacename>
```

See [DROP TABLESPACE](sql_commands/DROP_TABLESPACE.html) for more information.

## DROP TYPE 

Removes a data type.

```
DROP TYPE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP TYPE](sql_commands/DROP_TYPE.html) for more information.

## DROP USER 

Removes a database role.

```
DROP USER [IF EXISTS] <name> [, ...]
```

See [DROP USER](sql_commands/DROP_USER.html) for more information.

## DROP VIEW 

Removes a view.

```
DROP VIEW [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

See [DROP VIEW](sql_commands/DROP_VIEW.html) for more information.

## END 

Commits the current transaction.

```
END [WORK | TRANSACTION]
```

See [END](sql_commands/END.html) for more information.

## EXECUTE 

Executes a prepared SQL statement.

```
EXECUTE <name> [ (<parameter> [, ...] ) ]
```

See [EXECUTE](sql_commands/EXECUTE.html) for more information.

## EXPLAIN 

Shows the query plan of a statement.

```
EXPLAIN [ANALYZE] [VERBOSE] <statement>
```

See [EXPLAIN](sql_commands/EXPLAIN.html) for more information.

## FETCH 

Retrieves rows from a query using a cursor.

```
FETCH [ <forward_direction> { FROM | IN } ] <cursorname>
```

See [FETCH](sql_commands/FETCH.html) for more information.

## GRANT 

Defines access privileges.

```
GRANT { {SELECT | INSERT | UPDATE | DELETE | REFERENCES | 
TRIGGER | TRUNCATE } [,...] | ALL [PRIVILEGES] }
    ON [TABLE] <tablename> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { {USAGE | SELECT | UPDATE} [,...] | ALL [PRIVILEGES] }
    ON SEQUENCE <sequencename> [, ...]
    TO { <rolename> | PUBLIC } [, ...] [WITH GRANT OPTION]

GRANT { {CREATE | CONNECT | TEMPORARY | TEMP} [,...] | ALL 
[PRIVILEGES] }
    ON DATABASE <dbname> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN DATA WRAPPER <fdwname> [, ...]
    TO { [ GROUP ] <rolename> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { USAGE | ALL [ PRIVILEGES ] }
    ON FOREIGN SERVER <servername> [, ...]
    TO { [ GROUP ] <rolename> | PUBLIC } [, ...] [ WITH GRANT OPTION ]

GRANT { EXECUTE | ALL [PRIVILEGES] }
    ON FUNCTION <funcname> ( [ [<argmode>] [<argname>] <argtype> [, ...] 
] ) [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { USAGE | ALL [PRIVILEGES] }
    ON LANGUAGE <langname> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { {CREATE | USAGE} [,...] | ALL [PRIVILEGES] }
    ON SCHEMA <schemaname> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT { CREATE | ALL [PRIVILEGES] }
    ON TABLESPACE <tablespacename> [, ...]
    TO {<rolename> | PUBLIC} [, ...] [WITH GRANT OPTION]

GRANT <parent_role> [, ...] 
    TO <member_role> [, ...] [WITH ADMIN OPTION]

GRANT { SELECT | INSERT | ALL [PRIVILEGES] } 
    ON PROTOCOL <protocolname>
    TO <username>
```

See [GRANT](sql_commands/GRANT.html) for more information.

## INSERT 

Creates new rows in a table.

```
INSERT INTO <table> [( <column> [, ...] )]
   {DEFAULT VALUES | VALUES ( {<expression> | DEFAULT} [, ...] ) 
   [, ...] | <query>}
```

See [INSERT](sql_commands/INSERT.html) for more information.

## LOAD 

Loads or reloads a shared library file.

```
LOAD '<filename>'
```

See [LOAD](sql_commands/LOAD.html) for more information.

## LOCK 

Locks a table.

```
LOCK [TABLE] name [, ...] [IN <lockmode> MODE] [NOWAIT]
```

See [LOCK](sql_commands/LOCK.html) for more information.

## MOVE 

Positions a cursor.

```
MOVE [ <forward_direction> {FROM | IN} ] <cursorname>
```

See [MOVE](sql_commands/MOVE.html) for more information.

## PREPARE 

Prepare a statement for execution.

```
PREPARE <name> [ (<datatype> [, ...] ) ] AS <statement>
```

See [PREPARE](sql_commands/PREPARE.html) for more information.

## REASSIGN OWNED 

Changes the ownership of database objects owned by a database role.

```
REASSIGN OWNED BY <old_role> [, ...] TO <new_role>
```

See [REASSIGN OWNED](sql_commands/REASSIGN_OWNED.html) for more information.

## REINDEX 

Rebuilds indexes.

```
REINDEX {INDEX | TABLE | DATABASE | SYSTEM} <name>
```

See [REINDEX](sql_commands/REINDEX.html) for more information.

## RELEASE SAVEPOINT 

Destroys a previously defined savepoint.

```
RELEASE [SAVEPOINT] <savepoint_name>
```

See [RELEASE SAVEPOINT](sql_commands/RELEASE_SAVEPOINT.html) for more information.

## RESET 

Restores the value of a system configuration parameter to the default value.

```
RESET <configuration_parameter>

RESET ALL
```

See [RESET](sql_commands/RESET.html) for more information.

## REVOKE 

Removes access privileges.

```
REVOKE [GRANT OPTION FOR] { {SELECT | INSERT | UPDATE | DELETE 
       | REFERENCES | TRIGGER | TRUNCATE } [,...] | ALL [PRIVILEGES] }
       ON [TABLE] <tablename> [, ...]
       FROM {<rolename> | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] { {USAGE | SELECT | UPDATE} [,...] 
       | ALL [PRIVILEGES] }
       ON SEQUENCE <sequencename> [, ...]
       FROM { <rolename> | PUBLIC } [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] { {CREATE | CONNECT 
       | TEMPORARY | TEMP} [,...] | ALL [PRIVILEGES] }
       ON DATABASE <dbname> [, ...]
       FROM {rolename | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] {EXECUTE | ALL [PRIVILEGES]}
       ON FUNCTION <funcname> ( [[<argmode>] [<argname>] <argtype>
                              [, ...]] ) [, ...]
       FROM {<rolename> | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] {USAGE | ALL [PRIVILEGES]}
       ON LANGUAGE <langname> [, ...]
       FROM {<rolename> | PUBLIC} [, ...]
       [ CASCADE | RESTRICT ]

REVOKE [GRANT OPTION FOR] { {CREATE | USAGE} [,...] 
       | ALL [PRIVILEGES] }
       ON SCHEMA <schemaname> [, ...]
       FROM {<rolename> | PUBLIC} [, ...]
       [CASCADE | RESTRICT]

REVOKE [GRANT OPTION FOR] { CREATE | ALL [PRIVILEGES] }
       ON TABLESPACE <tablespacename> [, ...]
       FROM { <rolename> | PUBLIC } [, ...]
       [CASCADE | RESTRICT]

REVOKE [ADMIN OPTION FOR] <parent_role> [, ...] 
       FROM <member_role> [, ...]
       [CASCADE | RESTRICT]
```

See [REVOKE](sql_commands/REVOKE.html) for more information.

## ROLLBACK 

Aborts the current transaction.

```
ROLLBACK [WORK | TRANSACTION]
```

See [ROLLBACK](sql_commands/ROLLBACK.html) for more information.

## ROLLBACK TO SAVEPOINT 

Rolls back the current transaction to a savepoint.

```
ROLLBACK [WORK | TRANSACTION] TO [SAVEPOINT] <savepoint_name>
```

See [ROLLBACK TO SAVEPOINT](sql_commands/ROLLBACK_TO_SAVEPOINT.html) for more information.

## SAVEPOINT 

Defines a new savepoint within the current transaction.

```
SAVEPOINT <savepoint_name>
```

See [SAVEPOINT](sql_commands/SAVEPOINT.html) for more information.

## SELECT 

Retrieves rows from a table or view.

```
[ WITH [ RECURSIVE1 ] <with_query> [, ...] ]
SELECT [ALL | DISTINCT [ON (<expression> [, ...])]]
  * | <expression >[[AS] <output_name>] [, ...]
  [FROM <from_item> [, ...]]
  [WHERE <condition>]
  [GROUP BY <grouping_element> [, ...]]
  [HAVING <condition> [, ...]]
  [WINDOW <window_name> AS (<window_specification>)]
  [{UNION | INTERSECT | EXCEPT} [ALL] <select>]
  [ORDER BY <expression> [ASC | DESC | USING <operator>] [NULLS {FIRST | LAST}] [, ...]]
  [LIMIT {<count> | ALL}]
  [OFFSET <start>]
  [FOR {UPDATE | SHARE} [OF <table_name> [, ...]] [NOWAIT] [...]]
```

See [SELECT](sql_commands/SELECT.html) for more information.

## SELECT INTO 

Defines a new table from the results of a query.

```
[ WITH [ RECURSIVE1 ] <with_query> [, ...] ]
SELECT [ALL | DISTINCT [ON ( <expression> [, ...] )]]
    * | <expression> [AS <output_name>] [, ...]
    INTO [TEMPORARY | TEMP] [TABLE] <new_table>
    [FROM <from_item> [, ...]]
    [WHERE <condition>]
    [GROUP BY <expression> [, ...]]
    [HAVING <condition> [, ...]]
    [{UNION | INTERSECT | EXCEPT} [ALL] <select>]
    [ORDER BY <expression> [ASC | DESC | USING <operator>] [NULLS {FIRST | LAST}] [, ...]]
    [LIMIT {<count> | ALL}]
    [OFFSET <start>]
    [FOR {UPDATE | SHARE} [OF <table_name> [, ...]] [NOWAIT] 
    [...]]
```

See [SELECT INTO](sql_commands/SELECT_INTO.html) for more information.

## SET 

Changes the value of a Greenplum Database configuration parameter.

```
SET [SESSION | LOCAL] <configuration_parameter> {TO | =} <value> | 
    '<value>' | DEFAULT}

SET [SESSION | LOCAL] TIME ZONE {<timezone> | LOCAL | DEFAULT}
```

See [SET](sql_commands/SET.html) for more information.

## SET ROLE 

Sets the current role identifier of the current session.

```
SET [SESSION | LOCAL] ROLE <rolename>

SET [SESSION | LOCAL] ROLE NONE

RESET ROLE
```

See [SET ROLE](sql_commands/SET_ROLE.html) for more information.

## SET SESSION AUTHORIZATION 

Sets the session role identifier and the current role identifier of the current session.

```
SET [SESSION | LOCAL] SESSION AUTHORIZATION <rolename>

SET [SESSION | LOCAL] SESSION AUTHORIZATION DEFAULT

RESET SESSION AUTHORIZATION
```

See [SET SESSION AUTHORIZATION](sql_commands/SET_SESSION_AUTHORIZATION.html) for more information.

## SET TRANSACTION 

Sets the characteristics of the current transaction.

```
SET TRANSACTION [<transaction_mode>] [READ ONLY | READ WRITE]

SET SESSION CHARACTERISTICS AS TRANSACTION <transaction_mode> 
     [READ ONLY | READ WRITE]
```

See [SET TRANSACTION](sql_commands/SET_TRANSACTION.html) for more information.

## SHOW 

Shows the value of a system configuration parameter.

```
SHOW <configuration_parameter>

SHOW ALL
```

See [SHOW](sql_commands/SHOW.html) for more information.

## START TRANSACTION 

Starts a transaction block.

```
START TRANSACTION [SERIALIZABLE | READ COMMITTED | READ UNCOMMITTED]
                  [READ WRITE | READ ONLY]
```

See [START TRANSACTION](sql_commands/START_TRANSACTION.html) for more information.

## TRUNCATE 

Empties a table of all rows.

```
TRUNCATE [TABLE] <name> [, ...] [CASCADE | RESTRICT]
```

See [TRUNCATE](sql_commands/TRUNCATE.html) for more information.

## UPDATE 

Updates rows of a table.

```
UPDATE [ONLY] <table> [[AS] <alias>]
   SET {<column> = {<expression> | DEFAULT} |
   (<column> [, ...]) = ({<expression> | DEFAULT} [, ...])} [, ...]
   [FROM <fromlist>]
   [WHERE <condition >| WHERE CURRENT OF <cursor_name> ]
```

See [UPDATE](sql_commands/UPDATE.html) for more information.

## VACUUM 

Garbage-collects and optionally analyzes a database.

```
VACUUM [FULL] [FREEZE] [VERBOSE] [<table>]

VACUUM [FULL] [FREEZE] [VERBOSE] ANALYZE
              [<table> [(<column> [, ...] )]]
```

See [VACUUM](sql_commands/VACUUM.html) for more information.

## VALUES 

Computes a set of rows.

```
VALUES ( <expression> [, ...] ) [, ...]
   [ORDER BY <sort_expression> [ASC | DESC | USING <operator>] [, ...]]
   [LIMIT {<count> | ALL}] [OFFSET <start>]
```

See [VALUES](sql_commands/VALUES.html) for more information.

**Parent topic:** [SQL Command Reference](sql_commands/sql_ref.html)

