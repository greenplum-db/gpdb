# ALTER TEXT SEARCH PARSER 

## <a id="Description"></a>Description 

Changes the definition of a text search parser.

## <a id="Synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER TEXT SEARCH PARSER <name> RENAME TO <new_name>
ALTER TEXT SEARCH PARSER <name> SET SCHEMA <new_schema>
```

## <a id="section3"></a>Description 

`ALTER TEXT SEARCH PARSER` changes the definition of a text search parser. Currently, the only supported functionality is to change the parser's name.

You must be a superuser to use `ALTER TEXT SEARCH PARSER`.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing text search parser.

new\_name
:   The new name of the text search parser.

new\_schema
:   The new schema for the text search parser.

## <a id="section7"></a>Compatibility 

There is no `ALTER TEXT SEARCH PARSER` statement in the SQL standard.

## <a id="section8"></a>See Also 

[CREATE TEXT SEARCH PARSER](CREATE_TEXT_SEARCH_PARSER.html), [DROP TEXT SEARCH PARSER](DROP_TEXT_SEARCH_PARSER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

