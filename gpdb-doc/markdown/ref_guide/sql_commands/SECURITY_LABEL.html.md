# SECURITY LABEL 

Defines or changes a security label applied to an object.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
SECURITY LABEL [ FOR <provider> ] ON
{
  TABLE <object_name> |
  COLUMN <table_name>.<column_name> |
  AGGREGATE <aggregate_name> ( <aggregate_signature> ) |
  DATABASE <object_name> |
  DOMAIN <object_name> |
  EVENT TRIGGER <object_name> |
  FOREIGN TABLE <object_name>
  FUNCTION <function_name> [ ( [ [ <argmode> ] [ <argname> ] <argtype> [, ...] ] ) ] |
  MATERIALIZED VIEW <object_name> |
  [ PROCEDURAL ] LANGUAGE <object_name> |
  PROCEDURE <procedure_name> [ ( [ [ <argmode> ] [ <argname> ] <argtype> [, ...] ] ) ] |
  ROLE <object_name |
  ROUTINE <routine_name> [ ( [ [ <argmode> ] [ <argname> ] <argtype> [, ...] ] ) ] |
  SCHEMA <object_name> |
  SEQUENCE <object_name> |
  TABLESPACE <object_name> |
  TYPE <object_name> |
  VIEW <object_name>
} IS { <string_literal> | NULL }

where <aggregate_signature> is:

  * |
  [ <argmode> ] [ <argname> ] <argtype> [ , ... ] |
  [ [ <argmode> ] [ <argname> ] <argtype> [ , ... ] ] ORDER BY [ <argmode> ] [ <argname> ] <argtype> [ , ... ]
```

## <a id="section3"></a>Description 

`SECURITY LABEL` applies a security label to a database object. You can associate an arbitrary number of security labels, one per label provider, with a given database object. Label providers are loadable modules that register themselves with Greenplum Database.

The label provider determines whether a given label is valid and whether it is permissible to assign that label to a given object. The meaning of a given label is label provider-specific. Greenplum Database places no restrictions on whether or how a label provider must interpret security labels; it merely provides a mechanism for storing them. In practice, this facility is intended to allow integration with label-based mandatory access control (MAC) systems. Such systems make all access control decisions based on object labels, rather than traditional discretionary access control (DAC) concepts such as users and groups.

## <a id="section4"></a>Parameters 

object_name
table_name.column_name
aggregate_name
function_name
procedure_name
routine_name
:   The name of the object to be labeled. You can schema-qualify the names of objects that reside in schemas (tables, functions, etc.).

provider
:   The name of the label provider with which to associate this label. The named provider must be loaded and must consent to the proposed labeling operation. If exactly one provider is loaded, you can omit the provider name.

argmode
:   The mode of a function, procedure, or aggregate argument: `IN`, `OUT`, `INOUT`, or `VARIADIC`. If omitted, the default is `IN`. Note that `SECURITY LABEL` does not pay attention to `OUT` arguments, because only the input arguments are needed to determine the function's identity. It is sufficient to list only the `IN`, `INOUT`, and `VARIADIC` arguments.

argname
:   The name of a function, procedure, or aggregate argument. Note that `SECURITY LABEL` does not pay attention to argument names, because only the argument data types are required to determine the function's identity.

argtype
:   The data type of a function, procedure, or aggregate argument.

PROCEDURAL
:   Greenplum Database ignores this noise word.

string_literal
:   The new setting of the security label, written as a string literal.

NULL
:   Specify `NULL` to drop the security label.


## <a id="section6"></a>Examples 

The following example sets or changes the security label of a table when a label provider named `selinix` is loaded:

``` sql
SECURITY LABEL FOR selinux ON TABLE mytable IS 'system_u:object_r:sepgsql_table_t:s0';
```

To remove the label from the table:

``` sql
SECURITY LABEL FOR selinux ON TABLE mytable IS NULL;
```

## <a id="section7"></a>Compatibility 

There is no `SECURITY LABEL` command in the the SQL standard.


**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

