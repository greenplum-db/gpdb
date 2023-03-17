# ALTER LANGUAGE 

Changes the definition of a procedural language.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER [ PROCEDURAL ] LANGUAGE <name> RENAME TO <new_name>
ALTER [ PROCEDURAL ] LANGUAGE <name> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }
```

## <a id="section3"></a>Description 

`ALTER LANGUAGE` changes the definition of a procedural language for a specific database. Definition changes supported include renaming the language or assigning a new owner. You must be superuser or the owner of the language to use `ALTER LANGUAGE`.

## <a id="section4"></a>Parameters 

name
:   Name of a language.

new\_name
:   The new name of the language.

new\_owner
:   The new owner of the language.

## <a id="section5"></a>Compatibility 

There is no `ALTER LANGUAGE` statement in the SQL standard.

## <a id="section6"></a>See Also 

[CREATE LANGUAGE](CREATE_LANGUAGE.html), [DROP LANGUAGE](DROP_LANGUAGE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

