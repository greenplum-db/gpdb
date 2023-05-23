# SAVEPOINT 

Defines a new savepoint within the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
SAVEPOINT <savepoint_name>
```

## <a id="section3"></a>Description 

`SAVEPOINT` establishes a new savepoint within the current transaction.

A savepoint is a special mark inside a transaction that allows all commands that are run after it was established to be rolled back, restoring the transaction state to what it was at the time of the savepoint.

## <a id="section4"></a>Parameters 

savepoint\_name
:   The name of the new savepoint. If savepoints with the same name already exist, they are inaccessible until newer identically-named savepoints are released.

## <a id="section5"></a>Notes 

Use [ROLLBACK TO SAVEPOINT](ROLLBACK_TO_SAVEPOINT.html) to rollback to a savepoint. Use [RELEASE SAVEPOINT](RELEASE_SAVEPOINT.html) to destroy a savepoint, keeping the effects of commands run after it was established.

Savepoints can be established only inside a transaction block. You can define multiple savepoints within a transaction.

## <a id="section6"></a>Examples 

To establish a savepoint and later undo the effects of all commands run after it was established:

```
BEGIN;
    INSERT INTO table1 VALUES (1);
    SAVEPOINT my_savepoint;
    INSERT INTO table1 VALUES (2);
    ROLLBACK TO SAVEPOINT my_savepoint;
    INSERT INTO table1 VALUES (3);
COMMIT;
```

The above transaction inserts the values 1 and 3, but not 2.

To establish and later destroy a savepoint:

```
BEGIN;
    INSERT INTO table1 VALUES (3);
    SAVEPOINT my_savepoint;
    INSERT INTO table1 VALUES (4);
    RELEASE SAVEPOINT my_savepoint;
COMMIT;
```

The above transaction inserts both 3 and 4.

To use a single savepoint name:

``` sql
BEGIN;
    INSERT INTO table1 VALUES (1);
    SAVEPOINT my_savepoint;
    INSERT INTO table1 VALUES (2);
    SAVEPOINT my_savepoint;
    INSERT INTO table1 VALUES (3);

    -- rollback to the second savepoint
    ROLLBACK TO SAVEPOINT my_savepoint;
    SELECT * FROM table1;               -- shows rows 1 and 2

    -- release the second savepoint
    RELEASE SAVEPOINT my_savepoint;

    -- rollback to the first savepoint
    ROLLBACK TO SAVEPOINT my_savepoint;
    SELECT * FROM table1;               -- shows only row 1
COMMIT;
```

The above transaction shows row 3 being rolled back first, then row 2.

## <a id="section7"></a>Compatibility 

SQL requires a savepoint to be destroyed automatically when another savepoint with the same name is established. In Greenplum Database, the old savepoint is kept, though only the more recent one is used when rolling back or releasing. \(Releasing the newer savepoint will cause the older one to again become accessible to [ROLLBACK TO SAVEPOINT](ROLLBACK_TO_SAVEPOINT.html) and [RELEASE SAVEPOINT](RELEASE_SAVEPOINT.html).\) Otherwise, `SAVEPOINT` is fully SQL conforming.

## <a id="section8"></a>See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [RELEASE SAVEPOINT](RELEASE_SAVEPOINT.html), [ROLLBACK](ROLLBACK.html), [ROLLBACK TO SAVEPOINT](ROLLBACK_TO_SAVEPOINT.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

