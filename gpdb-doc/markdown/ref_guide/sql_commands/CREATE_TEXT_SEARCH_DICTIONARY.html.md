# CREATE TEXT SEARCH DICTIONARY 

Defines a new text search dictionary.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE TEXT SEARCH DICTIONARY <name> (
    TEMPLATE = <template>
    [, <option> = <value> [, ... ]]
)
```

## <a id="section3"></a>Description 

CREATE TEXT SEARCH DICTIONARY creates a new text search dictionary. A text search dictionary specifies a way of recognizing interesting or uninteresting words for searching. A dictionary depends on a text search template, which specifies the functions that actually perform the work. Typically the dictionary provides some options that control the detailed behavior of the template's functions.

If a schema name is given then the text search dictionary is created in the specified schema. Otherwise it is created in the current schema.

The user who defines a text search dictionary becomes its owner.

Refer to [Using Full Text Search](../../admin_guide/textsearch/full-text-search.html#full-text-search) for further information.

## <a id="section4"></a>Parameters 

name
:   The name of the text search dictionary to be created. The name can be schema-qualified.

template
:   The name of the text search template that will define the basic behavior of this dictionary.

option
:   The name of a template-specific option to be set for this dictionary.

value
:   The value to use for a template-specific option. If the value is not a simple identifier or number, it must be quoted \(but you can always quote it, if you wish\).

The options can appear in any order.

## <a id="section5"></a>Examples 

The following example command creates a Snowball-based dictionary with a nonstandard list of stop words.

```
CREATE TEXT SEARCH DICTIONARY my_russian (
    template = snowball,
    language = russian,
    stopwords = myrussian
);
```

## <a id="section6"></a>Compatibility 

There is no `CREATE TEXT SEARCH DICTIONARY` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER TEXT SEARCH DICTIONARY](ALTER_TEXT_SEARCH_DICTIONARY.html), [DROP TEXT SEARCH DICTIONARY](DROP_TEXT_SEARCH_DICTIONARY.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

