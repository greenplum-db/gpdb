# DROP TEXT SEARCH PARSER 

## <a id="Description"></a>Description 

Removes a text search parser.

## <a id="Synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
DROP TEXT SEARCH PARSER [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP TEXT SEARCH PARSER` drops an existing text search parser. You must be a superuser to use this command.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the text search parser does not exist. Greenplum Database issues a notice in this case.

name
:   The name \(optionally schema-qualified\) of an existing text search parser.

CASCADE
:   Automatically drop objects that depend on the text search parser, and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the text search parser if any objects depend on it. This is the default.

## <a id="Examples"></a>Examples 

Remove the text search parser `my_parser`:

```
DROP TEXT SEARCH PARSER my_parser;
```

This command will not succeed if there are any existing text search configurations that use the parser. Add `CASCADE` to drop such configurations along with the parser.

## <a id="section7"></a>Compatibility 

There is no `DROP TEXT SEARCH PARSER` statement in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER TEXT SEARCH PARSER](ALTER_TEXT_SEARCH_PARSER.html), [CREATE TEXT SEARCH PARSER](CREATE_TEXT_SEARCH_PARSER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

