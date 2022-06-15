# MOVE 

Positions a cursor.

## Synopsis 

``` {#sql_command_synopsis}
MOVE [ <forward_direction> {FROM | IN} ] <cursorname>
```

where forward\_direction can be empty or one of:

```
    NEXT
    FIRST
    LAST
    ABSOLUTE <count>
    RELATIVE <count>
    <count>
    ALL
    FORWARD
    FORWARD <count>
    FORWARD ALL
```

## Description 

`MOVE` repositions a cursor without retrieving any data. `MOVE` works exactly like the [FETCH](FETCH.html) command, except it only positions the cursor and does not return rows.

Note that it is not possible to move a cursor position backwards in Greenplum Database, since scrollable cursors are not supported. You can only move a cursor forward in position using `MOVE`.

**Outputs**

On successful completion, a `MOVE` command returns a command tag of the form

```
MOVE <count>
```

The count is the number of rows that a `FETCH` command with the same parameters would have returned \(possibly zero\).

## Parameters 

forward\_direction
:   See [FETCH](FETCH.html) for more information.

cursorname
:   The name of an open cursor.

## Examples 

-- Start the transaction:

```
BEGIN;
```

-- Set up a cursor:

```
DECLARE mycursor CURSOR FOR SELECT * FROM films;
```

-- Move forward 5 rows in the cursor `mycursor`:

```
MOVE FORWARD 5 IN mycursor;
MOVE 5
```

--Fetch the next row after that \(row 6\):

```
FETCH 1 FROM mycursor;
 code  | title  | did | date_prod  |  kind  |  len
-------+--------+-----+------------+--------+-------
 P_303 | 48 Hrs | 103 | 1982-10-22 | Action | 01:37
(1 row)
```

-- Close the cursor and end the transaction:

```
CLOSE mycursor;
COMMIT;
```

## Compatibility 

There is no `MOVE` statement in the SQL standard.

## See Also 

[DECLARE](DECLARE.html), [FETCH](FETCH.html), [CLOSE](CLOSE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

