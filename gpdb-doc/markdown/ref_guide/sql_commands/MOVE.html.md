# MOVE 

Positions a cursor.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
MOVE [ <forward_direction> [ FROM | IN ] ] <cursor_name>
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

## <a id="section3"></a>Description 

`MOVE` repositions a cursor without retrieving any data. `MOVE` works exactly like the [FETCH](FETCH.html) command, except it only positions the cursor and does not return rows.

> **Note** You cannot `MOVE` a `PARALLEL RETRIEVE CURSOR`.

It is not possible to move a cursor position backwards in Greenplum Database, since scrollable cursors are not supported. You can only move a cursor forward in position using `MOVE`.

**Outputs**

On successful completion, a `MOVE` command returns a command tag of the form

```
MOVE <count>
```

The count is the number of rows that a `FETCH` command with the same parameters would have returned \(possibly zero\).

## <a id="section5"></a>Parameters 

forward\_direction
:   The parameters for the `MOVE` command are identical to those of the `FETCH` command; refer to [FETCH](FETCH.html) for details on syntax and usage.

cursor\_name
:   The name of an open cursor.

## <a id="section6"></a>Examples 

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

-- Fetch the next row after that \(row 6\):

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

## <a id="section7"></a>Compatibility 

There is no `MOVE` statement in the SQL standard.

## <a id="section8"></a>See Also 

[DECLARE](DECLARE.html), [FETCH](FETCH.html), [CLOSE](CLOSE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

