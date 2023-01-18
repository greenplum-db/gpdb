# CREATE ACCESS METHOD

Defines a new access method.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE ACCESS METHOD <name>
    TYPE <access_method_type>
    HANDLER <handler_function>
```

## <a id="section3"></a>Description 

`CREATE ACCESS METHOD` creates a new access method.

The access method name must be unique within the database.

Only superusers can define new access methods.


## <a id="section4"></a>Parameters 

name
:   The name of the access method to create.

access\_method\_type
:   The type of access method to define. Only `TABLE` and `INDEX` types are supported at present.

handler\_function
:   handler\_function is the name \(possibly schema-qualified\) of a previously registered function that represents the access method. The handler function must be declared to take a single argument of type `internal`, and its return type depends on the type of access method; for `TABLE` access methods, it must be `table_am_handler` and for `INDEX` access methods, it must be `index_am_handler`. The C-level API that the handler function must implement varies depending on the type of access method. The table access method API is described in [Table Access Method Interface Definition](https://www.postgresql.org/docs/12/tableam.html) in the PostgreSQL documentation. The index access method API is described in [Index Access Method Interface Definition](https://www.postgresql.org/docs/12/indexam.html).

## <a id="section6"></a>Examples 

Create an index access method `heptree` with handler function `heptree_handler`:

``` sql
CREATE ACCESS METHOD heptree TYPE INDEX HANDLER heptree_handler;
```

## <a id="section7"></a>Compatibility 

`CREATE ACCESS METHOD` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[DROP ACCESS METHOD](DROP_ACCESS_METHOD.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [CREATE OPERATOR FAMILY](CREATE_OPERATOR_FAMILY.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

