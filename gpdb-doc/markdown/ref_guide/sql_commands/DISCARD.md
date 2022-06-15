# DISCARD 

Discards the session state.

## Synopsis 

``` {#sql_command_synopsis}
DISCARD { ALL | PLANS | TEMPORARY | TEMP }
```

## Description 

`DISCARD` releases internal resources associated with a database session. This command is useful for partially or fully resetting the session's state. There are several subcommands to release different types of resources. `DISCARD ALL` is not supported by Greenplum Database.

## Parameters 

PLANS
:   Releases all cached query plans, forcing re-planning to occur the next time the associated prepared statement is used.

SEQUENCES
:   Discards all cached sequence-related state, including any preallocated sequence values that have not yet been returned by `nextval()`. \(See `CREATE SEQUENCE` for a description of preallocated sequence values.\)

TEMPORARY/TEMP
:   Drops all temporary tables created in the current session.

ALL
:   Releases all temporary resources associated with the current session and resets the session to its initial state.

    **Note:** Greenplum Database does not support `DISCARD ALL` and returns a notice message if you attempt to run the command.

:   As an alternative, you can the run following commands to release temporary session resources:

    ```
    SET SESSION AUTHORIZATION DEFAULT;
    RESET ALL;
    DEALLOCATE ALL;
    CLOSE ALL;
    SELECT pg_advisory_unlock_all();
    DISCARD PLANS;
    DISCARD SEQUENCES;
    DISCARD TEMP;
    ```

## Compatibility 

`DISCARD` is a Greenplum Database extension.

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

