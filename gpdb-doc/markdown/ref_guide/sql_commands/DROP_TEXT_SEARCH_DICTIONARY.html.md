# DROP TEXT SEARCH DICTIONARY 

Removes a text search dictionary.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP TEXT SEARCH DICTIONARY [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP TEXT SEARCH DICTIONARY` drops an existing text search dictionary. You must be the owner of the dictionary to run this command.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the text search dictionary does not exist. Greenplum Database issues a notice in this case.

name
:   The name \(optionally schema-qualified\) of an existing text search dictionary.

CASCADE
:   Automatically drop objects that depend on the text search dictionary, and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the text search dictionary if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the text search dictionary `english`:

```
DROP TEXT SEARCH DICTIONARY english;
```

This command will not succeed if there are any existing text search configurations that use the dictionary. Add `CASCADE` to drop such configurations along with the dictionary.

## <a id="section6"></a>Compatibility 

There is no `CREATE TEXT SEARCH DICTIONARY` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER TEXT SEARCH DICTIONARY](ALTER_TEXT_SEARCH_DICTIONARY.html), [CREATE TEXT SEARCH DICTIONARY](CREATE_TEXT_SEARCH_DICTIONARY.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

