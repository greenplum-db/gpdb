# DROP DATABASE 

Removes a database.

## Synopsis 

``` {#sql_command_synopsis}
DROP DATABASE [IF EXISTS] <name>
```

## Description 

`DROP DATABASE` drops a database. It removes the catalog entries for the database and deletes the directory containing the data. It can only be executed by the database owner. Also, it cannot be executed while you or anyone else are connected to the target database. \(Connect to `postgres` or any other database to issue this command.\)

**Warning:** `DROP DATABASE` cannot be undone. Use it with care!

## Parameters 

IF EXISTS
:   Do not throw an error if the database does not exist. A notice is issued in this case.

name
:   The name of the database to remove.

## Notes 

`DROP DATABASE` cannot be executed inside a transaction block.

This command cannot be executed while connected to the target database. Thus, it might be more convenient to use the program `dropdb` instead, which is a wrapper around this command.

## Examples 

Drop the database named `testdb`:

```
DROP DATABASE testdb;
```

## Compatibility 

There is no `DROP DATABASE` statement in the SQL standard.

## See Also 

[ALTER DATABASE](ALTER_DATABASE.html), [CREATE DATABASE](CREATE_DATABASE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

