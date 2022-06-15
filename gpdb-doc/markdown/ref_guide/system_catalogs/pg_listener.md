# pg\_listener 

The `pg_listener` system catalog table supports the `LISTEN` and `NOTIFY` commands. A listener creates an entry in `pg_listener` for each notification name it is listening for. A notifier scans and updates each matching entry to show that a notification has occurred. The notifier also sends a signal \(using the PID recorded in the table\) to awaken the listener from sleep.

This table is not currently used in Greenplum Database.

|column|type|references|description|
|------|----|----------|-----------|
|`relname`|name| |Notify condition name. \(The name need not match any actual relation in the database.|
|`listenerpid`|int4| |PID of the server process that created this entry.|
|`notification`|int4| |Zero if no event is pending for this listener. If an event is pending, the PID of the server process that sent the notification.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

