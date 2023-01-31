# CREATE TEXT SEARCH CONFIGURATION 

Defines a new text search configuration.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE TEXT SEARCH CONFIGURATION <name> (
    PARSER = <parser_name> |
    COPY = <source_config>
)
```

## <a id="section3"></a>Description 

`CREATE TEXT SEARCH CONFIGURATION` creates a new text search configuration. A text search configuration specifies a text search parser that can divide a string into tokens, plus dictionaries that can be used to determine which tokens are of interest for searching.

If only the parser is specified, then the new text search configuration initially has no mappings from token types to dictionaries, and therefore will ignore all words. Subsequent [ALTER TEXT SEARCH CONFIGURATION](ALTER_TEXT_SEARCH_CONFIGURATION.html) commands must be used to create mappings to make the configuration useful. Alternatively, an existing text search configuration can be copied.

If a schema name is provided then the text search configuration is created in the specified schema. Otherwise it is created in the current schema.

The user who defines a text search configuration becomes its owner.

Refer to [Using Full Text Search](../../admin_guide/textsearch/full-text-search.html#full-text-search) for further information.

## <a id="section4"></a>Parameters 

name
:   The name of the text search configuration to be created. The name can be schema-qualified.

parser\_name
:   The name of the text search parser to use for this configuration.

source\_config
:   The name of an existing text search configuration to copy.

## <a id="section9"></a>Notes 

The `PARSER` and `COPY` options are mutually exclusive, because when an existing configuration is copied, its parser selection is copied too.

## <a id="section6"></a>Compatibility 

There is no `CREATE TEXT SEARCH CONFIGURATION` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER TEXT SEARCH CONFIGURATION](ALTER_TEXT_SEARCH_CONFIGURATION.html), [DROP TEXT SEARCH CONFIGURATION](DROP_TEXT_SEARCH_CONFIGURATION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

