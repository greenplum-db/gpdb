---
title: Example of Managing Transaction IDs 
---

For Greenplum Database, the transaction ID \(XID\) value an incrementing 32-bit \(2<sup>32</sup>\) value. The maximum unsigned 32-bit value is 4,294,967,295, or about four billion. The XID values restart at 3 after the maximum is reached. Greenplum Database handles the limit of XID values with two features:

-   Calculations on XID values using modulo-2<sup>32</sup> arithmetic that allow Greenplum Database to reuse XID values. The modulo calculations determine the order of transactions, whether one transaction has occurred before or after another, based on the XID.

    Every XID value can have up to two billion \(2<sup>31</sup>\) XID values that are considered previous transactions and two billion \(231 -1 \) XID values that are considered newer transactions. The XID values can be considered a circular set of values with no endpoint similar to a 24 hour clock.

    Using the Greenplum Database modulo calculations, as long as two XIDs are within 2<sup>31</sup> transactions of each other, comparing them yields the correct result.

-   A frozen XID value that Greenplum Database uses as the XID for current \(visible\) data rows. Setting a row's XID to the frozen XID performs two functions.
    -   When Greenplum Database compares XIDs using the modulo calculations, the frozen XID is always smaller, earlier, when compared to any other XID. If a row's XID is not set to the frozen XID and 2<sup>31</sup> new transactions are run, the row appears to be run in the future based on the modulo calculation.
    -   When the row's XID is set to the frozen XID, the original XID can be used, without duplicating the XID. This keeps the number of data rows on disk with assigned XIDs below \(2<sup>32</sup>\).

**Note:** Greenplum Database assigns XID values only to transactions that involve DDL or DML operations, which are typically the only transactions that require an XID.

**Parent topic:**[About Concurrency Control in Greenplum Database](../intro/about_mvcc.html)

## <a id="topic_zsw_yck_wv"></a>Simple MVCC Example 

This is a simple example of the concepts of a MVCC database and how it manages data and transactions with transaction IDs. This simple MVCC database example consists of a single table:

-   The table is a simple table with 2 columns and 4 rows of data.
-   The valid transaction ID \(XID\) values are from 0 up to 9, after 9 the XID restarts at 0.
-   The frozen XID is -2. This is different than the Greenplum Database frozen XID.
-   Transactions are performed on a single row.
-   Only insert and update operations are performed.
-   All updated rows remain on disk, no operations are performed to remove obsolete rows.

The example only updates the amount values. No other changes to the table.

The example shows these concepts.

-   [How transaction IDs are used to manage multiple, simultaneous transactions on a table.](#transactions)
-   [How transaction IDs are managed with the frozen XID](#managin_xid)
-   [How the modulo calculation determines the order of transactions based on transaction IDs](#table_itw_yck_wv)

### <a id="transactions"></a>Managing Simultaneous Transactions 

This table is the initial table data on disk with no updates. The table contains two database columns for transaction IDs, `xmin` \(transaction that created the row\) and `xmax` \(transaction that updated the row\). In the table, changes are added, in order, to the bottom of the table.

|item|amount|xmin|xmax|
|----|------|----|----|
|widget|100|0|null|
|giblet|200|1|null|
|sprocket|300|2|null|
|gizmo|400|3|null|

The next table shows the table data on disk after some updates on the amount values have been performed.

-   xid = 4: `update tbl set amount=208 where item = 'widget'`
-   xid = 5: `update tbl set amount=133 where item = 'sprocket'`
-   xid = 6: `update tbl set amount=16 where item = 'widget'`

In the next table, the bold items are the current rows for the table. The other rows are obsolete rows, table data that on disk but is no longer current. Using the xmax value, you can determine the current rows of the table by selecting the rows with `null` value. Greenplum Database uses a slightly different method to determine current table rows.

|item|amount|xmin|xmax|
|----|------|----|----|
|widget|100|0|4|
|**giblet**|**200**|1|**null**|
|sprocket|300|2|5|
|**gizmo**|**400**|3|**null**|
|widget|208|4|6|
|**sprocket**|**133**|5|**null**|
|**widget**|**16**|6|**null**|

The simple MVCC database works with XID values to determine the state of the table. For example, both these independent transactions run concurrently.

-   `UPDATE` command changes the sprocket amount value to `133` \(xmin value `5`\)
-   `SELECT` command returns the value of sprocket.

During the `UPDATE` transaction, the database returns the value of sprocket `300`, until the `UPDATE` transaction completes.

### <a id="managin_xid"></a>Managing XIDs and the Frozen XID 

For this simple example, the database is close to running out of available XID values. When Greenplum Database is close to running out of available XID values, Greenplum Database takes these actions.

-   Greenplum Database issues a warning stating that the database is running out of XID values.

    ```
    WARNING: database "<database_name>" must be vacuumed within <number_of_transactions> transactions
    ```

-   Before the last XID is assigned, Greenplum Database stops accepting transactions to prevent assigning an XID value twice and issues this message.

    ```
    FATAL: database is not accepting commands to avoid wraparound data loss in database "<database_name>" 
    ```


To manage transaction IDs and table data that is stored on disk, Greenplum Database provides the `VACUUM` command.

-   A `VACUUM` operation frees up XID values so that a table can have more than 10 rows by changing the xmin values to the frozen XID.
-   A `VACUUM` operation manages obsolete or deleted table rows on disk. This database's `VACUUM` command changes the XID values `obsolete` to indicate obsolete rows. A Greenplum Database `VACUUM` operation, without the `FULL` option, deletes the data opportunistically to remove rows on disk with minimal impact to performance and data availability.

For the example table, a `VACUUM` operation has been performed on the table. The command updated table data on disk. This version of the `VACUUM` command performs slightly differently than the Greenplum Database command, but the concepts are the same.

-   For the widget and sprocket rows on disk that are no longer current, the rows have been marked as `obsolete`.
-   For the giblet and gizmo rows that are current, the xmin has been changed to the frozen XID.

    The values are still current table values \(the row's xmax value is `null`\). However, the table row is visible to all transactions because the xmin value is frozen XID value that is older than all other XID values when modulo calculations are performed.


After the `VACUUM` operation, the XID values `0`, `1`, `2`, and `3` available for use.

|item|amount|xmin|xmax|
|----|------|----|----|
|widget|100|obsolete|obsolete|
|**giblet**|**200**|-2|**null**|
|sprocket|300|obsolete|obsolete|
|**gizmo**|**400**|-2|**null**|
|widget|208|4|6|
|**sprocket**|**133**|5|**null**|
|**widget**|**16**|6|**null**|

When a row disk with the xmin value of `-2` is updated, the xmax value is replaced with the transaction XID as usual, and the row on disk is considered obsolete after any concurrent transactions that access the row have completed.

Obsolete rows can be deleted from disk. For Greenplum Database, the `VACUUM` command, with `FULL` option, does more extensive processing to reclaim disk space.

### <a id="modulo_calc"></a>Example of XID Modulo Calculations 

The next table shows the table data on disk after more `UPDATE` transactions. The XID values have rolled over and start over at `0`. No additional `VACUUM` operations have been performed.

|item|amount|xmin|xmax|
|----|------|----|----|
|widget|100|obsolete|obsolete|
|giblet|200|-2|1|
|sprocket|300|obsolete|obsolete|
|gizmo|400|-2|9|
|widget|208|4|6|
|**sprocket**|**133**|5|**null**|
|widget|16|6|7|
|**widget**|**222**|7|**null**|
|giblet|233|8|0|
|**gizmo**|**18**|9|**null**|
|giblet|88|0|1|
|**giblet**|**44**|1|**null**|

When performing the modulo calculations that compare XIDs, Greenplum Database, considers the XIDs of the rows and the current range of available XIDs to determine if XID wrapping has occurred between row XIDs.

For the example table XID wrapping has occurred. The XID `1` for giblet row is a later transaction than the XID `7` for widget row based on the modulo calculations for XID values even though the XID value `7` is larger than `1`.

For the widget and sprocket rows, XID wrapping has not occurred and XID `7` is a later transaction than XID `5`.

