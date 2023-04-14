# COMMENT 

Defines or changes the comment of an object.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
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

where agg\_signature is:

```
* |
[ <argmode> ] [ <argname> ] <argtype> [ , ... ] |
[ [ <argmode> ] [ <argname> ] <argtype> [ , ... ] ] ORDER BY [ <argmode> ] [ <argname> ] <argtype> [ , ... ]
```

## <a id="section3"></a>Description 

`COMMENT` stores a comment about a database object. Only one comment string is stored for each object. To remove a comment, write `NULL` in place of the text string. Comments are automatically dropped when the object is dropped.

For most kinds of object, only the object's owner can set the comment. Roles don't have owners, so the rule for `COMMENT ON ROLE` is that you must be superuser to comment on a superuser role, or have the `CREATEROLE` privilege to comment on non-superuser roles. Of course, a superuser can comment on anything.

Comments can be easily retrieved with the psql meta-commands `\dd`, `\d+`, and `\l+`. Other user interfaces to retrieve comments can be built atop the same built-in functions that psql uses, namely `obj_description`, `col_description`, and `shobj_description`.

## <a id="section4"></a>Parameters 

object\_name
relation\_name.column\_name
agg\_name
constraint\_name
func\_name
operator\_name
rule\_name
trigger\_name
:   The name of the object to be commented. Names of tables, aggregates, collations, conversions, domains, foreign tables, functions, indexes, operators, operator classes, operator families, sequences, text search objects, types, views, and materialized views can be schema-qualified. When commenting on a column, relation\_name must refer to a table, view, materialized view, composite type, or foreign table.

    > **Note** Greenplum Database does not support triggers.

source\_type
:   The name of the source data type of the cast.

target\_type
:   The name of the target data type of the cast.

argmode
:   The mode of a function or aggregate argument: either `IN`, `OUT`, `INOUT`, or `VARIADIC`. If omitted, the default is `IN`. Note that `COMMENT` does not actually pay any attention to `OUT` arguments, since only the input arguments are needed to determine the function's identity. So it is sufficient to list the `IN`, `INOUT`, and `VARIADIC` arguments.

argname
:   The name of a function or aggregate argument. Note that `COMMENT ON FUNCTION` does not actually pay any attention to argument names, since only the argument data types are needed to determine the function's identity.

argtype
:   The data type of a function or aggregate argument.

large\_object\_oid
:   The OID of the large object.

:   > **Note** Greenplum Database does not support the PostgreSQL [large object facility](https://www.postgresql.org/docs/12/largeobjects.html) for streaming user data that is stored in large-object structures.

left\_type
right\_type
:   The data type\(s\) of the operator's arguments \(optionally schema-qualified\). Write `NONE` for the missing argument of a prefix or postfix operator.

PROCEDURAL
:   This is a noise word.

text
:   The new comment, written as a string literal; or `NULL` to drop the comment.

## <a id="section5"></a>Notes 

There is presently no security mechanism for viewing comments: any user connected to a database can see all the comments for objects in that database. For shared objects such as databases, roles, and tablespaces, comments are stored globally so any user connected to any database in the cluster can see all the comments for shared objects. Therefore, do not put security-critical information in comments.

## <a id="section6"></a>Examples 

Attach a comment to the table `mytable`:

```
COMMENT ON TABLE mytable IS 'This is my table.';
```

Remove it again:

```
COMMENT ON TABLE mytable IS NULL;
```

Some more examples:

```
COMMENT ON AGGREGATE my_aggregate (double precision) IS 'Computes sample variance';
COMMENT ON CAST (text AS int4) IS 'Allow casts from text to int4';
COMMENT ON COLLATION "fr_CA" IS 'Canadian French';
COMMENT ON COLUMN my_table.my_column IS 'Employee ID number';
COMMENT ON CONVERSION my_conv IS 'Conversion to UTF8';
COMMENT ON CONSTRAINT bar_col_cons ON bar IS 'Constrains column col';
COMMENT ON DATABASE my_database IS 'Development Database';
COMMENT ON DOMAIN my_domain IS 'Email Address Domain';
COMMENT ON EXTENSION hstore IS 'implements the hstore data type';
COMMENT ON FOREIGN DATA WRAPPER mywrapper IS 'my foreign data wrapper';
COMMENT ON FOREIGN TABLE my_foreign_table IS 'Employee Information in other database';
COMMENT ON FUNCTION my_function (timestamp) IS 'Returns Roman Numeral';
COMMENT ON INDEX my_index IS 'Enforces uniqueness on employee ID';
COMMENT ON LANGUAGE plpython IS 'Python support for stored procedures';
COMMENT ON LARGE OBJECT 346344 IS 'Planning document';
COMMENT ON OPERATOR ^ (text, text) IS 'Performs intersection of two texts';
COMMENT ON OPERATOR - (NONE, integer) IS 'Unary minus';
COMMENT ON OPERATOR CLASS int4ops USING btree IS '4 byte integer operators for btrees';
COMMENT ON OPERATOR FAMILY integer_ops USING btree IS 'all integer operators for btrees';
COMMENT ON ROLE my_role IS 'Administration group for finance tables';
COMMENT ON RULE my_rule ON my_table IS 'Logs updates of employee records';
COMMENT ON SCHEMA my_schema IS 'Departmental data';
COMMENT ON SEQUENCE my_sequence IS 'Used to generate primary keys';
COMMENT ON SERVER myserver IS 'my foreign server';
COMMENT ON TABLE my_schema.my_table IS 'Employee Information';
COMMENT ON TABLESPACE my_tablespace IS 'Tablespace for indexes';
COMMENT ON TEXT SEARCH CONFIGURATION my_config IS 'Special word filtering';
COMMENT ON TEXT SEARCH DICTIONARY swedish IS 'Snowball stemmer for Swedish language';
COMMENT ON TEXT SEARCH PARSER my_parser IS 'Splits text into words';
COMMENT ON TEXT SEARCH TEMPLATE snowball IS 'Snowball stemmer';
COMMENT ON TRIGGER my_trigger ON my_table IS 'Used for RI';
COMMENT ON TYPE complex IS 'Complex number data type';
COMMENT ON VIEW my_view IS 'View of departmental costs';
```

## <a id="section7"></a>Compatibility 

There is no `COMMENT` statement in the SQL standard.

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

