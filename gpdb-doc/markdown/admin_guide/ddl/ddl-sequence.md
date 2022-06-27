# Creating and Using Sequences 

A Greenplum Database sequence object is a special single row table that functions as a number generator. You can use a sequence to generate unique integer identifiers for a row that you add to a table. Declaring a column of type `SERIAL` implicitly creates a sequence counter for use in that table column.

Greenplum Database provides commands to create, alter, and drop a squence. Greenplum Database also provides built-in functions to return the next value in the sequence \(`nextval()`\) or to set the sequence to a specific start value \(`setval()`\).

**Note:** The PostgreSQL `currval()` and `lastval()` sequence functions are not supported in Greenplum Database.

Attributes of a sequence object include the name of the sequence, its increment value, and the last, minimum, and maximum values of the sequence counter. Sequences also have a special boolean attribute named `is_called` that governs the auto-increment behavior of a `nextval()` operation on the sequence counter. When a sequence's `is_called` attribute is `true`, `nextval()` increments the sequence counter before returning the value. When the `is_called` attribute value of a sequence is `false`, `nextval()` does not increment the counter before returning the value.

**Parent topic:** [Defining Database Objects](../ddl/ddl.html)

## Creating a Sequence 

The [CREATE SEQUENCE](../../ref_guide/sql_commands/CREATE_SEQUENCE.html) command creates and initializes a sequence with the given sequence name and optional start value. The sequence name must be distinct from the name of any other sequence, table, index, or view in the same schema. For example:

```
CREATE SEQUENCE myserial START 101;

```

When you create a new sequence, Greenplum Database sets the sequence `is_called` attribute to `false`. Invoking `nextval()` on a newly-created sequence does not increment the sequence counter, but returns the sequence start value and sets `is_called` to `true`.

## Using a Sequence 

After you create a sequence with the `CREATE SEQUENCE` command, you can examine the sequence and use the sequence built-in functions.

### Examining Sequence Attributes 

To examine the current attributes of a sequence, query the sequence directly. For example, to examine a sequence named `myserial`:

```
SELECT * FROM myserial;

```

### Returning the Next Sequence Counter Value 

You can invoke the `nextval()` built-in function to return and use the next value in a sequence. The following command inserts the next value of the sequence named `myserial` into the first column of a table named `vendors`:

```
INSERT INTO vendors VALUES (nextval('myserial'), 'acme');

```

`nextval()` uses the sequence's `is_called` attribute value to determine whether or not to increment the sequence counter before returning the value. `nextval()` advances the counter when `is_called` is `true`. `nextval()` sets the sequence `is_called` attribute to `true` before returning.

A `nextval()` operation is never rolled back. A fetched value is considered used, even if the transaction that performed the `nextval()` fails. This means that failed transactions can leave unused holes in the sequence of assigned values.

**Note:** You cannot use the `nextval()` function in `UPDATE` or `DELETE` statements if mirroring is enabled in Greenplum Database.

### Setting the Sequence Counter Value 

You can use the Greenplum Database `setval()` built-in function to set the counter value for a sequence. For example, the following command sets the counter value of the sequence named `myserial` to `201`:

```
SELECT setval('myserial', 201);

```

`setval()` has two function signatures: `setval(sequence, start_val)` and `setval(sequence, start_val, is_called)`. The default behaviour of `setval( sequence, start_val)` sets the sequence `is_called` attribute value to `true`.

If you do not want the sequence counter advanced on the next `nextval()` call, use the `setval( sequence, start_val, is_called)` function signature, passing a `false` argument:

```
SELECT setval('myserial', 201, false);

```

`setval()` operations are never rolled back.

## Altering a Sequence 

The [ALTER SEQUENCE](../../ref_guide/sql_commands/ALTER_SEQUENCE.html) command changes the attributes of an existing sequence. You can alter the sequence minimum, maximum, and increment values. You can also restart the sequence at a specific value.

Any parameters not set in the `ALTER SEQUENCE` command retain their prior settings.

The following command restarts the sequence named `myserial` at value `105`:

```
ALTER SEQUENCE myserial RESTART WITH 105;

```

When you alter a sequence start value with the `ALTER SEQUENCE` command, Greenplum Database sets the sequence's `is_called` attribute to `false`. The first `nextval()` invoked after restarting a sequence does not advance the sequence counter, but returns the sequence restart value and sets `is_called` to `true`.

## Dropping a Sequence 

The [DROP SEQUENCE](../../ref_guide/sql_commands/DROP_SEQUENCE.html) command removes a sequence. For example, the following command removes the sequence named `myserial`:

```
DROP SEQUENCE myserial;

```

## Specifying a Sequence as the Default Value for a Column 

You can reference a sequence directly in the [CREATE TABLE](../../ref_guide/sql_commands/CREATE_TABLE.html) command in addition to using the `SERIAL` or `BIGSERIAL` types. For example:

```
CREATE TABLE tablename ( id INT4 DEFAULT nextval('myserial'), name text );

```

You can also alter a table column to set its default value to a sequence counter:

```
ALTER TABLE tablename ALTER COLUMN id SET DEFAULT nextval('myserial');

```

## Sequence Wraparound 

By default, a sequence does not wrap around. That is, when a sequence reaches the max value \(`+32767` for `SMALLSERIAL`, `+2147483647` for `SERIAL`, `+9223372036854775807` for `BIGSERIAL`\), every subsequent `nextval()` call produces an error. You can alter a sequence to make it cycle around and start at `1` again:

```
ALTER SEQUENCE myserial CYCLE;
```

You can also specify the wraparound behaviour when you create the sequence:

```
CREATE SEQUENCE myserial CYCLE;
```

