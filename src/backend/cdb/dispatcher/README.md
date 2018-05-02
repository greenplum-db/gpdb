## Dispatcher API
This document illustrates the interfaces of a GPDB component called dispatcher, which is responsible for
1) building connections from master node to segments,
2) managing query/plan dispatching, and
3) collecting query execution results.

The implementation of dispatcher is mainly located under this directory.

### Terms Used:
* Gang: Gang refers to a group of processes on segments. There are 4 types of Gang:
	* `GANGTYPE_ENTRYDB_READER`: consist of one single process on master node
	* `GANGTYPE_SINGLETON_READER`: consist of one single process on a segment node
	* `GANGTYPE_PRIMARY_READER`: consist of N (number of segments) processes, each process is on a different segment
	* `GANGTYPE_PRIMARY_WRITER`: like `GANGTYPE_PRIMARY_READER`, while it can update segment databases, and is responsible for DTM (Distributed Transaction Management). A session can have at most one Gang of this type, and reader Gangs cannot exist without a writer Gang
<br><br>
For a query/plan, QD would build one `GANGTYPE_PRIMARY_WRITER` Gang, and several (0 included) reader Gangs based on the plan. A Gang could be reused across queries in a session. GPDB provides several GUCs to control Gang resuage, e.g, `gp_vmem_idle_resource_timeout`, `gp_cached_gang_threshold` and `gp_vmem_protect_gang_cache_limit`
<br><br>
* Dispatch: sending plan, utility statement, plain SQL text, and DTX command to Gangs, collecting results of execution and handling errors

### Interface Routines:
* Gang creation and tear down:
	* `AllocateReaderGang`: create Gang of type `GANGTYPE_ENTRYDB_READER`, `GANGTYPE_SINGLETON_READER`, `GANGTYPE_PRIMARY_READER` by specification. Gang reuage logic is included.
	* `AllocateWriterGang`: create Gang of type `GANGTYPE_PRIMARY_WRITER`
	* `DisconnectAndDestroyGang`: tear down a Gang of any type, but make sure no reader Gang exist before calling this routine for writer Gang
	* `DisconnectAndDestroyAllGangs`: tear down all existing Gangs of this session
* Gang status check:
	* `GangOK`: check if a created Gang is healthy
	* `GangsExist`: check if any Gang exists for this session
* Dispatch:
	* `CdbDispatchPlan`: send PlannedStmt to Gangs specified in `queryDesc` argument. Once finishes work on QD, call `CdbCheckDispatchResult` to wait results or `CdbDispatchHandleError` to cancel query on error
	* `CdbDispatchUtilityStatement`: send parsed utility statement to the writer Gang, and block to get results or error
	* `CdbDispatchCommand`: send plain SQL text to the writer Gang, and block to get results or error
	* `CdbDispatchSetCommand`: send SET commands to all existing Gangs except those allocated for extended queries, and block to get results or error
	* `CdbDispDtxProtocolCommand`: send DTX commands to the writer Gang, and block to get results or error
	
### Dispatcher Mode:
To improve parallelism, Dispatcher has two different implementations internally, one is using threads, the other leverages asynchronous network programming. When GUC `gp_connections_per_thread` is 0, async dispatcher is used, which is the default configuration
