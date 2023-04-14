# CREATE TEXT SEARCH PARSER 

## <a id="Description"></a>Description 

Defines a new text search parser.

## <a id="Synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE TEXT SEARCH PARSER <name> (
    START = <start_function> ,
    GETTOKEN = <gettoken_function> ,
    END = <end_function> ,
    LEXTYPES = <lextypes_function>
    [, HEADLINE = <headline_function> ]
)
```

## <a id="section3"></a>Description 

`CREATE TEXT SEARCH PARSER` creates a new text search parser. A text search parser defines a method for splitting a text string into tokens and assigning types \(categories\) to the tokens. A parser is not particularly useful by itself, but must be bound into a text search configuration along with some text search dictionaries to be used for searching.

If a schema name is given then the text search parser is created in the specified schema. Otherwise it is created in the current schema.

You must be a superuser to use `CREATE TEXT SEARCH PARSER`. \(This restriction is made because an erroneous text search parser definition could confuse or even crash the server.\)

Refer to [Using Full Text Search](../../admin_guide/textsearch/full-text-search.html#full-text-search) for further information.

## <a id="section4"></a>Parameters 

name
:   The name of the text search parser to be created. The name can be schema-qualified.

start\_function
:   The name of the start function for the parser.

gettoken\_function
:   The name of the get-next-token function for the parser.

end\_function
:   The name of the end function for the parser.

lextypes\_function
:   The name of the lextypes function for the parser \(a function that returns information about the set of token types it produces\).

headline\_function
:   The name of the headline function for the parser \(a function that summarizes a set of tokens\).

The function names can be schema-qualified if necessary. Argument types are not given, since the argument list for each type of function is predetermined. All except the headline function are required.

The arguments can appear in any order, not only the one shown above.

## <a id="section7"></a>Compatibility 

There is no `CREATE TEXT SEARCH PARSER` statement in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER TEXT SEARCH PARSER](ALTER_TEXT_SEARCH_PARSER.html), [DROP TEXT SEARCH PARSER](DROP_TEXT_SEARCH_PARSER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

