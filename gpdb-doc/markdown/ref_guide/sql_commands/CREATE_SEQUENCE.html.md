# CREATE SEQUENCE 

Defines a new sequence generator.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [TEMPORARY | TEMP] SEQUENCE [IF NOT EXISTS] <name>
       [ AS data_type ]
       [INCREMENT [BY] <increment>] 
       [MINVALUE <minvalue> | NO MINVALUE] 
       [MAXVALUE <maxvalue> | NO MAXVALUE] 
       [START [ WITH ] <start>] 
       [CACHE <cache>] 
       [[NO] CYCLE] 
       [OWNED BY { <table_name>.<column_name> | NONE }]
```

## <a id="section3"></a>Description 

`CREATE SEQUENCE` creates a new sequence number generator. This involves creating and initializing a new special single-row table with the same name. The generator will be owned by the user issuing the command.

If a schema name is given, then the sequence is created in the specified schema. Otherwise it is created in the current schema. Temporary sequences exist in a special schema, so you may not provide a schema name when creating a temporary sequence. The sequence name must be distinct from the name of any other sequence, table, index, view, or foreign table in the same schema.

After a sequence is created, you use the `nextval()` function to operate on the sequence. For example, to insert a row into a table that gets the next value of a sequence:

```
INSERT INTO distributors VALUES (nextval('myserial'), 'acme');
```

You can also use the function `setval()` to operate on a sequence, but only for queries that do not operate on distributed data. For example, the following query is allowed because it resets the sequence counter value for the sequence generator process on the coordinator:

```
SELECT setval('myserial', 201);
```

But the following query will be rejected in Greenplum Database because it operates on distributed data:

```
INSERT INTO product VALUES (setval('myserial', 201), 'gizmo');
```

In a regular \(non-distributed\) database, functions that operate on the sequence go to the local sequence table to get values as they are needed. In Greenplum Database, however, keep in mind that each segment is its own distinct database process. Therefore the segments need a single point of truth to go for sequence values so that all segments get incremented correctly and the sequence moves forward in the right order. A sequence server process runs on the coordinator and is the point-of-truth for a sequence in a Greenplum distributed database. Segments get sequence values at runtime from the coordinator.

Because of this distributed sequence design, there are some limitations on the functions that operate on a sequence in Greenplum Database:

-   `lastval()` and `currval()` functions are not supported.
-   `setval()` can only be used to set the value of the sequence generator on the coordinator, it cannot be used in subqueries to update records on distributed table data.
-   `nextval()` sometimes grabs a block of values from the coordinator for a segment to use, depending on the query. So values may sometimes be skipped in the sequence if all of the block turns out not to be needed at the segment level. Note that a regular PostgreSQL database does this too, so this is not something unique to Greenplum Database.

> **Note**
> The default sequence cache size in Greenplum Database is `20`.

Although you cannot update a sequence directly, you can use a query like:

```
SELECT * FROM <sequence_name>;
```

to examine the parameters and current state of a sequence. In particular, the last\_value field of the sequence shows the last value allocated by any session. \(Note that this value might be obsolete by the time it's printed, if other sessions are actively doing `nextval()` calls.\)

## <a id="section4"></a>Parameters 

TEMPORARY \| TEMP
:   If specified, the sequence object is created only for this session, and is automatically dropped on session exit. Existing permanent sequences with the same name are not visible \(in this session\) while the temporary sequence exists, unless they are referenced with schema-qualified names.

IF NOT EXISTS
:   Do not throw an error if a relation with the same name already exists. Greenplum Database issues a notice in this case. Note that there is no guarantee that the existing relation is anything like the sequence that would have been created - it might not even be a sequence.

name
:   The name \(optionally schema-qualified\) of the sequence to be created.

data\_type
:   The optional clause `AS data_type` specifies the data type of the sequence. Valid types are `smallint`, `integer`, and `bigint`. `bigint` is the default. The data type determines the default minimum and maximum values of the sequence.

increment
:   Specifies which value is added to the current sequence value to create a new value. A positive value will make an ascending sequence, a negative one a descending sequence. The default value is `1`.

minvalue
NO MINVALUE
:   Determines the minimum value a sequence can generate. If this clause is not supplied or `NO MINVALUE` is specified, then the default values will be used. The default for an ascending sequence is the maximum value of the data type. The default for a descending sequence is `-1`.

maxvalue
NO MAXVALUE
:   Determines the maximum value for the sequence. If this clause is not supplied or `NO MAXVALUE` is specified, then default values will be used. The defaults are 263-1 and -1 for ascending and descending sequences, respectively.

start
:   Allows the sequence to begin anywhere. The default starting value is `minvalue` for ascending sequences and `maxvalue` for descending ones.

cache
:   Specifies how many sequence numbers are to be preallocated and stored in memory for faster access. The default value is 20. The minimum value is 1 \(no cache\).
:   > **Note** When operating with a cache of sequence numbers (`cache > 1`), Greenplum Database may discard some cached sequence values. If you require consecutive values, you must explicitly set `CACHE 1` when you create or alter the sequence.

CYCLE
NO CYCLE
:   Allows the sequence to wrap around when the `maxvalue` \(for ascending\) or `minvalue` \(for descending\) has been reached. If the limit is reached, the next number generated will be the `minvalue` \(for ascending\) or `maxvalue` \(for descending\). If `NO CYCLE` is specified, any calls to `nextval()` after the sequence has reached its maximum value will return an error. If neither `CYCLE` or `NO CYCLE` are specified, `NO CYCLE` is the default.

OWNED BY table\_name.colume\_name
OWNED BY NONE
:   Causes the sequence to be associated with a specific table column, such that if that column \(or its whole table\) is dropped, the sequence will be automatically dropped as well. The specified table must have the same owner and be in the same schema as the sequence. `OWNED BY NONE`, the default, specifies that there is no such association.

## <a id="section5"></a>Notes 

Use `DROP SEQUENCE` to remove a sequence.

Sequences are based on bigint arithmetic, so the range cannot exceed the range of an eight-byte integer \(-9223372036854775808 to 9223372036854775807\).

Because `nextval()` and `setval()` calls are never rolled back, sequence objects cannot be used if "gapless" assignment of sequence numbers is needed. It is possible to build gapless assignment by using exclusive locking of a table containing a counter; but this solution is much more expensive than sequence objects, especially if many transactions need sequence numbers concurrently.

Although multiple sessions are guaranteed to allocate distinct sequence values, the values may be generated out of sequence when all the sessions are considered. For example, with a `cache` setting of `10`, session A might reserve values 1..10 and return `nextval=1`, then session B might reserve values 11..20 and return `nextval=11` before session A has generated nextval=2. Thus, with a `cache` setting of one it is safe to assume that `nextval()` values are generated sequentially; with a `cache` setting greater than one you should only assume that the `nextval()` values are all distinct, not that they are generated purely sequentially.

Another consideration is that a `setval()` executed on such a sequence will not be noticed by other sessions until they have used up any preallocated values they have cached.

## <a id="section6"></a>Examples 

Create an ascending sequence named `myseq`, starting at 101:

```
CREATE SEQUENCE myseq START 101;
```

Insert a row into a table that gets the next value of the sequence named `myseq`:

```
INSERT INTO distributors VALUES (nextval('myseq'), 'acme'); 
```

Reset the sequence counter value on the Greenplum Database coordinator:

```
SELECT setval('myseq', 201);
```

Illegal use of `setval()` in Greenplum Database \(setting sequence values on distributed data\):

```
INSERT INTO product VALUES (setval('myseq', 201), 'gizmo'); 
```

## <a id="section7"></a>Compatibility 

`CREATE SEQUENCE` conforms to the SQL standard, with the following exceptions:

-   You obtain the next value using the `nextval()` function instead of the `NEXT VALUE FOR` expression specified in the SQL standard.
-   The `OWNED BY` clause is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER SEQUENCE](ALTER_SEQUENCE.html), [DROP SEQUENCE](DROP_SEQUENCE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

