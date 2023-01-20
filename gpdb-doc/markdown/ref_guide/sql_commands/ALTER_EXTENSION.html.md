# ALTER EXTENSION 

Change the definition of an extension that is registered in a Greenplum database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
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

## <a id="section3"></a>Description 

`ALTER EXTENSION` changes the definition of an installed extension. These are the subforms:

UPDATE
:   This form updates the extension to a newer version. The extension must supply a suitable update script \(or series of scripts\) that can modify the currently-installed version into the requested version.

SET SCHEMA
:   This form moves the extension member objects into another schema. The extension must be *relocatable*.

ADD member\_object
:   This form adds an existing object to the extension. This is useful in extension update scripts. The added object is treated as a member of the extension. The object can only be dropped by dropping the extension.

DROP member\_object
:   This form removes a member object from the extension. This is mainly useful in extension update scripts. The object is not dropped, only disassociated from the extension.

See [Packaging Related Objects into an Extension](https://www.postgresql.org/docs/12/extend-extensions.html) for more information about these operations.

You must own the extension to use `ALTER EXTENSION`. The `ADD` and `DROP` forms also require ownership of the object that is being added or dropped.

## <a id="section4"></a>Parameters 

name
:   The name of an installed extension.

new\_version
:   The new version of the extension. The new\_version can be either an identifier or a string literal. If not specified, the command attempts to update to the default version in the extension control file.

new\_schema
:   The new schema for the extension.

object\_name
aggregate\_name
function\_name
operator\_name
:   The name of an object to be added to or removed from the extension. Names of tables, aggregates, domains, foreign tables, functions, operators, operator classes, operator families, sequences, text search objects, types, and views can be schema-qualified.

source\_type
:   The name of the source data type of the cast.

target\_type
:   The name of the target data type of the cast.

argmode
:   The mode of a function or aggregate argument: `IN`, `OUT`, `INOUT`, or `VARIADIC`. The default is `IN`.

    The command ignores the `OUT` arguments. Only the input arguments are required to determine the function identity. It is sufficient to list the `IN`, `INOUT`, and `VARIADIC` arguments.

argname
:   The name of a function or aggregate argument.

    The command ignores argument names, since only the argument data types are required to determine the function identity.

argtype
:   The data type of a function or aggregate argument.

left\_type
right\_type
:   The data types of the operator's arguments \(optionally schema-qualified\) . Specify `NONE` for the missing argument of a prefix or postfix operator.

PROCEDURAL
:   This is a noise word.

type\_name
:   The name of the data type of the transform.

lang\_name
:   The name of the language of the transform.

## <a id="section5"></a>Examples 

To update the hstore extension to version 2.0:

```
ALTER EXTENSION hstore UPDATE TO '2.0';
```

To change the schema of the `hstore` extension to `utils`:

```
ALTER EXTENSION hstore SET SCHEMA utils;
```

To add an existing function to the `hstore` extension:

```
ALTER EXTENSION hstore ADD FUNCTION populate_record(anyelement, hstore);
```

## <a id="section6"></a>Compatibility 

`ALTER EXTENSION` is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[CREATE EXTENSION](CREATE_EXTENSION.html), [DROP EXTENSION](DROP_EXTENSION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

