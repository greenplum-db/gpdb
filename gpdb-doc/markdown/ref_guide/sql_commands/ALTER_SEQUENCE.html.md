---
title: ALTER SEQUENCE 
---

Changes the definition of a sequence generator.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER SEQUENCE [ IF EXISTS ] <name> [INCREMENT [ BY ] <increment>] 
     [MINVALUE <minvalue> | NO MINVALUE] 
     [MAXVALUE <maxvalue> | NO MAXVALUE] 
     [START [ WITH ] <start> ]
     [RESTART [ [ WITH ] <restart>] ]
     [CACHE <cache>] [[ NO ] CYCLE] 
     [OWNED BY {<table.column> | NONE}]

ALTER SEQUENCE [ IF EXISTS ] <name> OWNER TO <new_owner>

ALTER SEQUENCE [ IF EXISTS ] <name> RENAME TO <new_name>

ALTER SEQUENCE [ IF EXISTS ] <name> SET SCHEMA <new_schema>
```

## <a id="section3"></a>Description 

`ALTER SEQUENCE` changes the parameters of an existing sequence generator. Any parameters not specifically set in the `ALTER SEQUENCE` command retain their prior settings.

You must own the sequence to use `ALTER SEQUENCE`. To change a sequence's schema, you must also have `CREATE` privilege on the new schema. Note that superusers have all these privileges automatically.

To alter the owner, you must be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the sequence's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the sequence. However, a superuser can alter ownership of any sequence anyway.\)

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of a sequence to be altered.

IF EXISTS
:   Do not throw an error if the sequence does not exist. A notice is issued in this case.

increment
:   The clause `INCREMENT BY increment` is optional. A positive value will make an ascending sequence, a negative one a descending sequence. If unspecified, the old increment value will be maintained.

minvalue
NO MINVALUE
:   The optional clause `MINVALUE minvalue` determines the minimum value a sequence can generate. If `NO MINVALUE` is specified, the defaults of 1 and -263-1 for ascending and descending sequences, respectively, will be used. If neither option is specified, the current minimum value will be maintained.

maxvalue
NO MAXVALUE
:   The optional clause `MAXVALUE maxvalue` determines the maximum value for the sequence. If `NO MAXVALUE` is specified, the defaults are 263-1 and -1 for ascending and descending sequences, respectively, will be used. If neither option is specified, the current maximum value will be maintained.

start
:   The optional clause START WITH start changes the recorded start value of the sequence. This has no effect on the *current* sequence value; it simply sets the value that future `ALTER SEQUENCE RESTART` commands will use.

restart
:   The optional clause `RESTART [ WITH restart ]` changes the current value of the sequence. This is equivalent to calling the `setval(sequence, start\_val, is\_called)` function with `is\_called = false`. The specified value will be returned by the next call of the `nextval(sequence)` function. Writing `RESTART` with no restart value is equivalent to supplying the start value that was recorded by `CREATE SEQUENCE` or last set by `ALTER SEQUENCE START WITH`.

new\_owner
:   The user name of the new owner of the sequence.

cache
:   The clause `CACHE cache` enables sequence numbers to be preallocated and stored in memory for faster access. The minimum value is 1 \(only one value can be generated at a time, i.e., no cache\). If unspecified, the old cache value will be maintained.

CYCLE
:   The optional `CYCLE` key word may be used to enable the sequence to wrap around when the maxvalue or minvalue has been reached by an ascending or descending sequence. If the limit is reached, the next number generated will be the respective minvalue or maxvalue.

NO CYCLE
:   If the optional `NO CYCLE` key word is specified, any calls to `nextval()` after the sequence has reached its maximum value will return an error. If neither `CYCLE` or `NO CYCLE` are specified, the old cycle behavior will be maintained.

OWNED BY table.column
OWNED BY NONE
:   The `OWNED BY` option causes the sequence to be associated with a specific table column, such that if that column \(or its whole table\) is dropped, the sequence will be automatically dropped as well. If specified, this association replaces any previously specified association for the sequence. The specified table must have the same owner and be in the same schema as the sequence. Specifying `OWNED BY NONE` removes any existing table column association.

new\_name
:   The new name for the sequence.

new\_schema
:   The new schema for the sequence.

## <a id="section5"></a>Notes 

To avoid blocking of concurrent transactions that obtain numbers from the same sequence, `ALTER SEQUENCE`'s effects on the sequence generation parameters are never rolled back; those changes take effect immediately and are not reversible. However, the `OWNED BY`, `OWNER TO`, `RENAME TO`, and `SET SCHEMA` clauses are ordinary catalog updates and can be rolled back.

`ALTER SEQUENCE` will not immediately affect `nextval()` results in sessions, other than the current one, that have preallocated \(cached\) sequence values. They will use up all cached values prior to noticing the changed sequence generation parameters. The current session will be affected immediately.

For historical reasons, `ALTER TABLE` can be used with sequences too; but the only variants of `ALTER TABLE` that are allowed with sequences are equivalent to the forms shown above.

## <a id="section6"></a>Examples 

Restart a sequence called `serial` at `105`:

```
ALTER SEQUENCE serial RESTART WITH 105;
```

## <a id="section7"></a>Compatibility 

`ALTER SEQUENCE` conforms to the SQL standard, except for the `START WITH`, `OWNED BY`, `OWNER TO`, `RENAME TO`, and `SET SCHEMA` clauses, which are Greenplum Database extensions.

## <a id="section8"></a>See Also 

[CREATE SEQUENCE](CREATE_SEQUENCE.html), [DROP SEQUENCE](DROP_SEQUENCE.html), [ALTER TABLE](ALTER_TABLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

