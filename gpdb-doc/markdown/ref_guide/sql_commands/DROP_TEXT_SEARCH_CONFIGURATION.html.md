# DROP TEXT SEARCH CONFIGURATION 

Removes a text search configuration.

## <a id="section2"></a>Synopsis 

```
DROP TEXT SEARCH CONFIGURATION [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP TEXT SEARCH CONFIGURATION` drops an existing text search configuration. You must be the owner of the configuration to run this command.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the text search configuration does not exist. Greenplum Database issues a notice in this case.

name
:   The name \(optionally schema-qualified\) of an existing text search configuration.

CASCADE
:   Automatically drop objects that depend on the text search configuration, and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the text search configuration if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the text search configuration `my_english`:

```
DROP TEXT SEARCH CONFIGURATION my_english;
```

This command will not succeed if there are any existing indexes that reference the configuration in `to_tsvector` calls. Add `CASCADE` to drop such indexes along with the text search configuration.

## <a id="section6"></a>Compatibility 

There is no `DROP TEXT SEARCH CONFIGURATION` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER TEXT SEARCH CONFIGURATION](ALTER_TEXT_SEARCH_CONFIGURATION.html), [CREATE TEXT SEARCH CONFIGURATION](CREATE_TEXT_SEARCH_CONFIGURATION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

