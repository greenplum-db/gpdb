# DROP TEXT SEARCH TEMPLATE 

## <a id="Description"></a>Description 

Removes a text search template.

## <a id="Synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
DROP TEXT SEARCH TEMPLATE [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP TEXT SEARCH TEMPLATE` drops an existing text search template. You must be a superuser to use this command.

You must be a superuser to use `ALTER TEXT SEARCH TEMPLATE`.

## <a id="section4"></a>Parameters 

`IF EXISTS`
:   Do not throw an error if the text search template does not exist. A notice is issued in this case.

`name`
:   The name \(optionally schema-qualified\) of an existing text search template.

`CASCADE`
:   Automatically drop objects that depend on the text search template.

`RESTRICT`
:   Refuse to drop the text search template if any objects depend on it. This is the default.

## <a id="section7"></a>Compatibility 

There is no `DROP TEXT SEARCH TEMPLATE` statement in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER TEXT SEARCH TEMPLATE](ALTER_TEXT_SEARCH_TEMPLATE.html), [CREATE TEXT SEARCH TEMPLATE](CREATE_TEXT_SEARCH_TEMPLATE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

