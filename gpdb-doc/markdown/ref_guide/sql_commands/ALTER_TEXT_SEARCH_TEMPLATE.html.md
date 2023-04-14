# ALTER TEXT SEARCH TEMPLATE 

## <a id="Description"></a>Description 

Changes the definition of a text search template.

## <a id="Synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER TEXT SEARCH TEMPLATE <name> RENAME TO <new_name>
ALTER TEXT SEARCH TEMPLATE <name> SET SCHEMA <new_schema>
```

## <a id="section3"></a>Description 

`ALTER TEXT SEARCH TEMPLATE` changes the definition of a text search parser. Currently, the only supported functionality is to change the template's name.

You must be a superuser to use `ALTER TEXT SEARCH TEMPLATE`.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing text search template.

new\_name
:   The new name of the text search template.

new\_schema
:   The new schema for the text search template.

## <a id="section7"></a>Compatibility 

There is no `ALTER TEXT SEARCH TEMPLATE` statement in the SQL standard.

## <a id="section8"></a>See Also 

[CREATE TEXT SEARCH TEMPLATE](CREATE_TEXT_SEARCH_TEMPLATE.html), [DROP TEXT SEARCH TEMPLATE](DROP_TEXT_SEARCH_TEMPLATE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

