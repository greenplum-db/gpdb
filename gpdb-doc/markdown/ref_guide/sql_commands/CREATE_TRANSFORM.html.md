# CREATE TRANSFORM

Defines a new transform

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [ OR REPLACE ] TRANSFORM FOR <type_name> LANGUAGE <lang_name> (
  FROM SQL WITH FUNCTION <from_sql_function_name> [ (<argument_type> [, ...]) ],
  TO SQL WITH FUNCTION <to_sql_function_name> [ (<argument_type> [, ...]) ]
);
```

## <a id="section3"></a>Description 

`CREATE TRANSFORM` defines a new transform. `CREATE OR REPLACE TRANSFORM` will either create a new transform, or replace an existing definition.

A transform specifies how to adapt a data type to a procedural language. For example, when writing a function in PL/Python using the `hstore` type, PL/Python has no prior knowledge how to present `hstore` values in the Python environment. Language implementations usually default to using the text representation, but that is inconvenient when, for example, an associative array or a list would be more appropriate.

A transform specifies two functions:

- A "from SQL" function that converts the type from the SQL environment to the language. This function will be invoked on the arguments of a function written in the language.

- A "to SQL" function that converts the type from the language to the SQL environment. This function will be invoked on the return value of a function written in the language.

It is not necessary to provide both of these functions. If one is not specified, the language-specific default behavior will be used if necessary. \(To prevent a transformation in a certain direction from happening at all, you could also write a transform function that always errors out.\)

To create a transform, you must own and have `USAGE` privilege on the type, have `USAGE` privilege on the language, and own and have `EXECUTE` privilege on the from-SQL and to-SQL functions, if specified.

## <a id="section4"></a>Parameters 

type\_name
:   The name of the data type of the transform.

lang\_name
:   The name of the language of the transform.

from\_sql\_function\_name[(argument\_type [, ...])]
:   The name of the function for converting the type from the SQL environment to the language. It must take a single argument of type `internal` and return type `internal`. The actual argument will be of the type for the transform, and the function should be coded as if it were. \(An SQL-level function returning `internal` must declare at least one argument of type `internal`.\) The actual return value will be something specific to the language implementation. If no argument list is specified, the function name must be unique in its schema.

to\_sql\_function\_name[(argument\_type [, ...])]
:   The name of the function for converting the type from the language to the SQL environment. It must take a single argument of type `internal` and return the type that is the type for the transform. The actual argument value will be something specific to the language implementation. If no argument list is specified, the function name must be unique in its schema.

## <a id="section5"></a>Notes 

Use [DROP TRANSFORM](DROP_TRANSFORM.html) to remove transforms.

## <a id="section6"></a>Examples 

To create a transform for type `hstore` and language `plpython3u`, first set up the type and the language:

``` sql
CREATE EXTENSION IF NOT EXISTS hstore;

CREATE EXTENSION plpython3u;
```

Then create the necessary functions:

``` sql
CREATE FUNCTION hstore_to_plpython(val internal) RETURNS internal
LANGUAGE C STRICT IMMUTABLE
AS ...;

CREATE FUNCTION plpython_to_hstore(val internal) RETURNS hstore
LANGUAGE C STRICT IMMUTABLE
AS ...;
```

And finally create the transform to connect them all together:

``` sql
CREATE TRANSFORM FOR hstore LANGUAGE plpython3u (
    FROM SQL WITH FUNCTION hstore_to_plpython(internal),
    TO SQL WITH FUNCTION plpython_to_hstore(internal)
);
```

In practice, these commands would be wrapped up in an extension.

## <a id="section7"></a>Compatibility 

This form of `CREATE TRANSFORM` is a Greenplum Database extension. There is a `CREATE TRANSFORM` command in the SQL standard, but it is for adapting data types to client languages. That usage is not supported by Greenplum.

## <a id="section8"></a>See Also 

[CREATE FUNCTION](CREATE_FUNCTION.html), [CREATE LANGUAGE](CREATE_LANGUAGE.html), [CREATE TYPE](CREATE_TYPE.html), [DROP TRANSFORM](DROP_TRANSFORM.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

