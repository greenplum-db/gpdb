# pg_prepared_statements

The `pg_prepared_statements` view displays all of the prepared statements that are available in the current session. See [PREPARE](../sql_commands/PREPARE.html) for more information about prepared statements.

`pg_prepared_statements` contains one row for each prepared statement. Rows are added to the view when a new prepared statement is created and removed when a prepared statement is released \(for example, with the [DEALLOCATE](../sql_commands/DEALLOCATE.html) command\).

The `pg_prepared_statements` view is read-only.

|name|type|description|
|----|----|----------|
|`name`|text|The identifier of the prepared statement.|
|`statement`|text| The query string submitted by the client to create this prepared statement. For prepared statements created via SQL, this is the `PREPARE` statement submitted by the client. For prepared statements created via the frontend/backend protocol, this is the text of the prepared statement itself. |
|`prepare_time`|timestamptz|The time at which the prepared statement was created.|
|`parameter_types`|regtype[]| The expected parameter types for the prepared statement in the form of an array of `regtype`. The object identifier corresponding to an element of this array can be obtained by casting the `regtype` value to `oid`. |
|`from_sql`|boolean|`true` if the prepared statement was created via the `PREPARE` SQL command, `false` if the statement was prepared via the frontend/backend protocol.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

