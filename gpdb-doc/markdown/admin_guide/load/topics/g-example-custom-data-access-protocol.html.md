---
title: Example Custom Data Access Protocol 
---

The following is the API for the Greenplum Database custom data access protocol. The example protocol implementation [gpextprotocal.c](g-gpextprotocal.c.html) is written in C and shows how the API can be used. For information about accessing a custom data access protocol, see [Using a Custom Protocol](g-using-a-custom-protocol.html).

```
/* ---- Read/Write function API ------*/
CALLED_AS_EXTPROTOCOL(fcinfo)
EXTPROTOCOL_GET_URL(fcinfo)(fcinfo) 
EXTPROTOCOL_GET_DATABUF(fcinfo) 
EXTPROTOCOL_GET_DATALEN(fcinfo) 
EXTPROTOCOL_GET_SCANQUALS(fcinfo) 
EXTPROTOCOL_GET_USER_CTX(fcinfo) 
EXTPROTOCOL_IS_LAST_CALL(fcinfo) 
EXTPROTOCOL_SET_LAST_CALL(fcinfo) 
EXTPROTOCOL_SET_USER_CTX(fcinfo, p)

/* ------ Validator function API ------*/
CALLED_AS_EXTPROTOCOL_VALIDATOR(fcinfo)
EXTPROTOCOL_VALIDATOR_GET_URL_LIST(fcinfo) 
EXTPROTOCOL_VALIDATOR_GET_NUM_URLS(fcinfo) 
EXTPROTOCOL_VALIDATOR_GET_NTH_URL(fcinfo, n) 
EXTPROTOCOL_VALIDATOR_GET_DIRECTION(fcinfo)
```

## <a id="notes1"></a>Notes 

The protocol corresponds to the example described in [Using a Custom Protocol](g-using-a-custom-protocol.html). The source code file name and shared object are `gpextprotocol.c` and `gpextprotocol.so`.

The protocol has the following properties:

-   The name defined for the protocol is `myprot`.
-   The protocol has the following simple form: the protocol name and a path, separated by `://`.

    `myprot://` `path`

-   Three functions are implemented:

    -   `myprot_import()` a read function
    -   `myprot_export()` a write function
    -   `myprot_validate_urls()` a validation function
    These functions are referenced in the `CREATE PROTOCOL` statement when the protocol is created and declared in the database.


The example implementation [gpextprotocal.c](g-gpextprotocal.c.html) uses `fopen()` and `fread()` to simulate a simple protocol that reads local files. In practice, however, the protocol would implement functionality such as a remote connection to some process over the network.

-   **[Installing the External Table Protocol](../../load/topics/g-installing-the-external-table-protocol.html)**  


**Parent topic:**[Loading and Unloading Data](../../load/topics/g-loading-and-unloading-data.html)

