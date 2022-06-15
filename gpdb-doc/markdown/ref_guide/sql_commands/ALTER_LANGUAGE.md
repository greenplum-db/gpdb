# ALTER LANGUAGE 

Changes the name of a procedural language.

## Synopsis 

``` {#sql_command_synopsis}
ALTER LANGUAGE <name> RENAME TO <newname>
ALTER LANGUAGE <name> OWNER TO <new_owner>
```

## Description 

`ALTER LANGUAGE` changes the definition of a procedural language for a specific database. Definition changes supported include renaming the language or assigning a new owner. You must be superuser or the owner of the language to use `ALTER LANGUAGE`.

## Parameters 

name
:   Name of a language.

newname
:   The new name of the language.

new\_owner
:   The new owner of the language.

## Compatibility 

There is no `ALTER LANGUAGE` statement in the SQL standard.

## See Also 

[CREATE LANGUAGE](CREATE_LANGUAGE.html), [DROP LANGUAGE](DROP_LANGUAGE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

