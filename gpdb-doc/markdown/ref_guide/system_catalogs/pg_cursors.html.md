# pg_cursors 

The `pg_cursors` view lists the currently available cursors. Cursors can be defined in one of the following ways:

-   via the DECLARE SQL statement
-   via the Bind message in the frontend/backend protocol
-   via the Server Programming Interface \(SPI\)

    > **Note** Greenplum Database does not support the definition, or access of, parallel retrieve cursors via SPI.


Cursors exist only for the duration of the transaction that defines them, unless they have been declared `WITH HOLD`. Non-holdable cursors are only present in the view until the end of their creating transaction.

> **Note** Greenplum Database does not support holdable parallel retrieve cursors.

|name|type|references|description|
|----|----|----------|-----------|
|`name`|text| |The name of the cursor.|
|`statement`|text| |The verbatim query string submitted to declare this cursor.|
|`is_holdable`|boolean| |`true` if the cursor is holdable \(that is, it can be accessed after the transaction that declared the cursor has committed\); `false` otherwise.<br/><br/>> **Note** Greenplum Database does not support holdable parallel retrieve cursors, this value is always `false` for such cursors.|
|`is_binary`|boolean| |`true` if the cursor was declared `BINARY`; `false` otherwise.|
|`is_scrollable`|boolean| |`true` if the cursor is scrollable \(that is, it allows rows to be retrieved in a nonsequential manner\); `false` otherwise.<br/><br/>> **Note** Greenplum Database does not support scrollable cursors, this value is always `false`.|
|`creation_time`|timestamptz| |The time at which the cursor was declared.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

