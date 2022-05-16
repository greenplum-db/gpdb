# pg_stat_last_operation 

The pg\_stat\_last\_operation table contains metadata tracking information about database objects \(tables, views, etc.\).

|column|type|references|description|
|------|----|----------|-----------|
|`classid`|oid|pg\_class.oid|OID of the system catalog containing the object.|
|`objid`|oid|any OID column|OID of the object within its system catalog.|
|`staactionname`|name| |The action that was taken on the object.|
|`stasysid`|oid|pg\_authid.oid|A foreign key to pg\_authid.oid.|
|`stausename`|name| |The name of the role that performed the operation on this object.|
|`stasubtype`|text| |The type of object operated on or the subclass of operation performed.|
|`statime`|timestamp with timezone| |The timestamp of the operation. This is the same timestamp that is written to the Greenplum Database server log files in case you need to look up more detailed information about the operation in the logs.|

The `pg_stat_last_operation` table contains metadata tracking information about operations on database objects. This information includes the object id, DDL action, user, type of object, and operation timestamp. Greenplum Database updates this table when a database object is created, altered, truncated, vacuumed, analyzed, or partitioned, and when privileges are granted to an object.

If you want to track the operations performed on a specific object, use the `objid` value. Because the `stasubtype` value can identify either the type of object operated on or the subclass of operation performed, it is not a suitable parameter when querying the `pg_stat_last_operation` table.

The following example creates and replaces a view, and then shows how to use `objid` as a query parameter on the `pg_stat_last_operation` table.

```
testdb=# CREATE VIEW trial AS SELECT * FROM gp_segment_configuration;
CREATE VIEW
testdb=# CREATE OR REPLACE VIEW trial AS SELECT * FROM gp_segment_configuration;
CREATE VIEW
testdb=# SELECT * FROM pg_stat_last_operation WHERE objid='trial'::regclass::oid;
 classid | objid | staactionname | stasysid | stausename | stasubtype |            statime            
---------+-------+---------------+----------+------------+------------+-------------------------------
  1259  | 24735 | CREATE         |       10 | gpadmin    | VIEW       | 2020-04-07 16:44:28.808811+00
  1259  | 24735 | ALTER          |       10 | gpadmin    | SET        | 2020-04-07 16:44:38.110615+00
(2 rows)
```

Notice that the `pg_stat_last_operation` table entry for the view `REPLACE` operation specifies the `ALTER` action \(`staactionname`\) and the `SET` subtype \(`stasubtype`\).

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

