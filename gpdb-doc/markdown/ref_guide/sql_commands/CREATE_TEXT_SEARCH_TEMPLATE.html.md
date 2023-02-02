# CREATE TEXT SEARCH TEMPLATE 

## <a id="Description"></a>Description 

Defines a new text search template.

## <a id="Synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE TEXT SEARCH TEMPLATE <name> (
    [ INIT = <init_function> , ]
    LEXIZE = <lexize_function>
)
```

## <a id="section3"></a>Description 

`CREATE TEXT SEARCH TEMPLATE` creates a new text search template. Text search templates define the functions that implement text search dictionaries. A template is not useful by itself, but must be instantiated as a dictionary to be used. The dictionary typically specifies parameters to be given to the template functions.

If a schema name is given then the text search template is created in the specified schema. Otherwise it is created in the current schema.

You must be a superuser to use `CREATE TEXT SEARCH TEMPLATE`. This restriction is made because an erroneous text search template definition could confuse or even crash the server. The reason for separating templates from dictionaries is that a template encapsulates the "unsafe" aspects of defining a dictionary. The parameters that can be set when defining a dictionary are safe for unprivileged users to set, and so creating a dictionary need not be a privileged operation.

Refer to [Using Full Text Search](../../admin_guide/textsearch/full-text-search.html#full-text-search) for further information.

## <a id="section4"></a>Parameters 

name
:   The name of the text search template to be created. The name can be schema-qualified.

init\_function
:   The name of the init function for the template.

lexize\_function
:   The name of the lexize function for the template.

The function names can be schema-qualified if necessary. Argument types are not given, since the argument list for each type of function is predetermined. The lexize function is required, but the init function is optional.

The arguments can appear in any order, not only the order shown above.

## <a id="section7"></a>Compatibility 

There is no `CREATE TEXT SEARCH TEMPLATE` statement in the SQL standard.

## <a id="section8"></a>See Also 

[DROP TEXT SEARCH TEMPLATE](DROP_TEXT_SEARCH_TEMPLATE.html), [ALTER TEXT SEARCH TEMPLATE](ALTER_TEXT_SEARCH_TEMPLATE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

