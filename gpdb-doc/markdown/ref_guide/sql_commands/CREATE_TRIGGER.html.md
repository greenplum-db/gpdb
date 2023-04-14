# CREATE TRIGGER 

Defines a new trigger. User-defined triggers are not supported in Greenplum Database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE TRIGGER <name> {BEFORE | AFTER} {<event> [OR ...]}
       ON <table> [ FOR [EACH] {ROW | STATEMENT} ]
       EXECUTE PROCEDURE <funcname> ( <arguments> )
```

## <a id="section3"></a>Description 

`CREATE TRIGGER` creates a new trigger. The trigger will be associated with the specified table and will run the specified function when certain events occur. If multiple triggers of the same kind are defined for the same event, they will be fired in alphabetical order by name.

>*Important* Due to the distributed nature of a Greenplum Database system, the use of triggers on data is very limited in Greenplum Database. The function used in the trigger must be `IMMUTABLE`, meaning it cannot use information not directly present in its argument list. The function specified in the trigger also cannot run any SQL or modify distributed database objects in any way. Given that triggers are most often used to alter tables \(for example, update these other rows when this row is updated\), these limitations offer very little practical use of triggers in Greenplum Database. For that reason, Greenplum does not support the use of user-defined triggers in Greenplum Database. Triggers cannot be used on append-optimized tables.
> Event Triggers, which capture only DDL events, _are_ supported in Greenplum Database. See the PostgreSQL documentation for [Event Triggers](https://www.postgresql.org/docs/12/event-triggers.html) for additional information.

[SELECT](SELECT.html) does not modify any rows so you can not create `SELECT` triggers. Rules and views are more appropriate in such cases.

## <a id="section4"></a>Parameters 

name
:   The name to give the new trigger. This must be distinct from the name of any other trigger for the same table.

BEFORE AFTER
:   Determines whether the function is called before or after the event. If the trigger fires before the event, the trigger may skip the operation for the current row, or change the row being inserted \(for `INSERT` and `UPDATE` operations only\). If the trigger fires after the event, all changes, including the last insertion, update, or deletion, are visible to the trigger.

event
:   Specifies the event that will fire the trigger \(`INSERT`, `UPDATE`, or `DELETE`\). Multiple events can be specified using OR.

table
:   The name \(optionally schema-qualified\) of the table the trigger is for.

FOR EACH ROW
FOR EACH STATEMENT
:   This specifies whether the trigger procedure should be fired once for every row affected by the trigger event, or just once per SQL statement. If neither is specified, `FOR EACH STATEMENT` is the default. A trigger that is marked `FOR EACH ROW` is called once for every row that the operation modifies. In contrast, a trigger that is marked `FOR EACH STATEMENT` only runs once for any given operation, regardless of how many rows it modifies.

funcname
:   A user-supplied function that is declared as `IMMUTABLE`, taking no arguments, and returning type `trigger`, which is run when the trigger fires. This function must not run SQL or modify the database in any way.

arguments
:   An optional comma-separated list of arguments to be provided to the function when the trigger is run. The arguments are literal string constants. Simple names and numeric constants may be written here, too, but they will all be converted to strings. Please check the description of the implementation language of the trigger function about how the trigger arguments are accessible within the function; it may be different from normal function arguments.

## <a id="section5"></a>Notes 

To create a trigger on a table, the user must have the `TRIGGER` privilege on the table.

## <a id="section6"></a>Examples 

Declare the trigger function and then a trigger:

```
CREATE FUNCTION sendmail() RETURNS trigger AS 
'$GPHOME/lib/emailtrig.so' LANGUAGE C IMMUTABLE;

CREATE TRIGGER t_sendmail AFTER INSERT OR UPDATE OR DELETE 
ON mytable FOR EACH STATEMENT EXECUTE PROCEDURE sendmail();
```

## <a id="section7"></a>Compatibility 

The `CREATE TRIGGER` statement in Greenplum Database implements a subset of the SQL standard. The following functionality is currently missing:

-   Greenplum Database has strict limitations on the function that is called by a trigger, which makes the use of triggers very limited in Greenplum Database. For this reason, triggers are not officially supported in Greenplum Database.
-   SQL allows triggers to fire on updates to specific columns \(e.g., `AFTER UPDATE OF col1, col2`\).
-   SQL allows you to define aliases for the 'old' and 'new' rows or tables for use in the definition of the triggered action \(e.g., `CREATE TRIGGER ... ON tablename REFERENCING OLD ROW AS somename NEW ROW AS othername ...`\). Since Greenplum Database allows trigger procedures to be written in any number of user-defined languages, access to the data is handled in a language-specific way.
-   Greenplum Database only allows the execution of a user-defined function for the triggered action. The standard allows the execution of a number of other SQL commands, such as `CREATE TABLE` as the triggered action. This limitation is not hard to work around by creating a user-defined function that runs the desired commands.
-   SQL specifies that multiple triggers should be fired in time-of-creation order. Greenplum Database uses name order, which was judged to be more convenient.
-   SQL specifies that `BEFORE DELETE` triggers on cascaded deletes fire after the cascaded `DELETE` completes. The Greenplum Database behavior is for `BEFORE DELETE` to always fire before the delete action, even a cascading one. This is considered more consistent.
-   The ability to specify multiple actions for a single trigger using `OR` is a Greenplum Database extension of the SQL standard.

## <a id="section8"></a>See Also 

[CREATE FUNCTION](CREATE_FUNCTION.html), [ALTER TRIGGER](ALTER_TRIGGER.html), [DROP TRIGGER](DROP_TRIGGER.html), [CREATE RULE](CREATE_RULE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

