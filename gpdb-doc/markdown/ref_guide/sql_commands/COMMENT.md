# COMMENT 

Defines or change the comment of an object.

## Synopsis 

``` {#sql_command_synopsis}
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

## Description 

`COMMENT` stores a comment about a database object. To modify a comment, issue a new `COMMENT` command for the same object. Only one comment string is stored for each object. To remove a comment, write `NULL` in place of the text string. Comments are automatically dropped when the object is dropped.

Comments can be easily retrieved with the psql meta-commands `\dd`, `\d+`, and `\l+`. Other user interfaces to retrieve comments can be built atop the same built-in functions that psql uses, namely `obj_description`, `col_description`, and `shobj_description`.

## Parameters 

object\_name
table\_name.column\_name
agg\_name
constraint\_name
func\_name
op
rule\_name
trigger\_name
:   The name of the object to be commented. Names of tables, aggregates, domains, functions, indexes, operators, operator classes, sequences, types, and views may be schema-qualified.

    **Note:** Greenplum Database does not support triggers.

agg\_type
:   An input data type on which the aggregate function operates. To reference a zero-argument aggregate function, write `*` in place of the list of input data types.

sourcetype
:   The name of the source data type of the cast.

targettype
:   The name of the target data type of the cast.

argmode
:   The mode of a function argument: either `IN`, `OUT`, `INOUT`, or `VARIADIC`. If omitted, the default is `IN`. Note that `COMMENT ON FUNCTION` does not actually pay any attention to `OUT` arguments, since only the input arguments are needed to determine the function's identity. So it is sufficient to list the `IN`, `INOUT`, and `VARIADIC` arguments.

argname
:   The name of a function argument. Note that `COMMENT ON FUNCTION` does not actually pay any attention to argument names, since only the argument data types are needed to determine the function's identity.

argtype
:   The data type\(s\) of the function's arguments \(optionally schema-qualified\), if any.

large\_object\_oid
:   The OID of the large object.

PROCEDURAL
:   This is a noise word.

text
:   The new comment, written as a string literal; or `NULL` to drop the comment.

## Notes 

There is presently no security mechanism for comments: any user connected to a database can see all the comments for objects in that database \(although only superusers can change comments for objects that they do not own\). For shared objects such as databases, roles, and tablespaces comments are stored globally and any user connected to any database can see all the comments for shared objects. Therefore, do not put security-critical information in comments.

## Examples 

Attach a comment to the table `mytable`:

```
COMMENT ON TABLE mytable IS 'This is my table.';
```

Remove it again:

```
COMMENT ON TABLE mytable IS NULL;
```

## Compatibility 

There is no `COMMENT` statement in the SQL standard.

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

