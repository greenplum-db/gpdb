# gp_segment_endpoints 

The `gp_segment_endpoints` view lists the endpoints created in the QE for all active parallel retrieve cursors declared by the current session user. When the Greenplum Database superuser accesses this view, it returns a list of all endpoints on the QE created for all parallel retrieve cursors declared by all users.

Endpoints exist only for the duration of the transaction that defines the parallel retrieve cursor, or until the cursor is closed.

|name|type|references|description|
|----|----|----------|-----------|
|auth\_token|text| |The authentication token for the retrieve session.|
|databaseid|oid| |The identifier of the database in which the parallel retrieve cursor was created.|
|senderpid|integer| |The identifier of the process sending the query results.|
|receiverpid|integer| |The process identifier of the retrieve session that is receiving the query results.|
|state|text| |The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|gp\_segment\_id|integer| |The QE's endpoint `gp_segment_id`.|
|sessionid|integer| |The identifier of the session in which the parallel retrieve cursor was created.|
|username|text| |The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|endpointname|text| |The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|
|cursorname|text| |The name of the parallel retrieve cursor.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

