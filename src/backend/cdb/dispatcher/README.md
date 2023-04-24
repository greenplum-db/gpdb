## Dispatcher API
This document illustrates the interfaces of a GPDB component called dispatcher, which is responsible for
1) building connections from coordinator node to segments,
2) managing query/plan dispatching, and
3) collecting query execution results.

The implementation of dispatcher is mainly located under this directory.

### Terms Used:
* QD: Query Dispatcher, a backend forked by coordinator with role GP_ROLE_DISPATCH who dispatches plan/command to QEs and get the results from QEs.
* QE: Query Executor, a backend forked by coordinator/segments with role GP_ROLE_EXECUTOR who gets plan/command from QD and pick a piece of it to execute.
* Gang: Gang refers to a group of processes on segments. There are 4 types of Gang:
	* `GANGTYPE_ENTRYDB_READER`: consist of one single process on coordinator node
	* `GANGTYPE_SINGLETON_READER`: consist of one single process on a segment node
	* `GANGTYPE_PRIMARY_READER`: consist of N (number of segments) processes, each process is on a different segment
	* `GANGTYPE_PRIMARY_WRITER`: like `GANGTYPE_PRIMARY_READER`, while it can update segment databases, and is responsible for DTM (Distributed Transaction Management). A session can have at most one Gang of this type, and reader Gangs cannot exist without a writer Gang
<br><br>
For a query/plan, QD would build one `GANGTYPE_PRIMARY_WRITER` Gang, and several (0 included) reader Gangs based on the plan. A Gang could be reused across queries in a session. GPDB provides several GUCs to control Gang resuage, e.g, `gp_vmem_idle_resource_timeout`, `gp_cached_gang_threshold` and `gp_vmem_protect_gang_cache_limit`
<br><br>
* Dispatch: sending plan, utility statement, plain SQL text, and DTX command to Gangs, collecting results of execution and handling errors

### Interface Routines:
* Gang creation and tear down:
	* `AllocateGang`: allocate a gang with specified gang type on specified segments, the gang is made up of idle QEs got from CdbComponentDatabases, according QE type is requested for gang types (see cdbcomponent_allocateIdleQE):
		* `GANGTYPE_ENTRYDB_READER`: SEGMENTTYPE_EXPLICT_ANY. 
		* `GANGTYPE_SINGLETON_READER`: SEGMENTTYPE_EXPLICT_ANY or SEGMENTTYPE_EXPLICT_READER for cursor.
		* `GANGTYPE_PRIMARY_READER`: SEGMENTTYPE_EXPLICT_ANY or SEGMENTTYPE_EXPLICT_READER for cursor.
		* `GANGTYPE_PRIMARY_WRITER`: SEGMENTTYPE_EXPLICT_WRITER.
	* `RecycleGang`: destroy or divide it into idle QEs pool, If gang can be cleanup correctly including discarding results, connection status check (see cdbcomponent_recycleIdleQE), otherwise, destroy it.
	* `DisconnectAndDestroyAllGangs`: destroy all existing Gangs of this session
* Gang status check:
	* `GangOK`: check if a created Gang is healthy
* Dispatch:
	* `CdbDispatchPlan`: send PlannedStmt to Gangs specified in `queryDesc` argument. Once finishes work on QD, call `CdbCheckDispatchResult` to wait results or `CdbDispatchHandleError` to cancel query on error
	* `CdbDispatchUtilityStatement`: send parsed utility statement to the writer Gang, and block to get results or error
	* `CdbDispatchCommand`: send plain SQL text to the writer Gang, and block to get results or error
	* `CdbDispatchSetCommand`: send SET commands to all existing Gangs except those allocated for extended queries, and block to get results or error
	* `CdbDispDtxProtocolCommand`: send DTX commands to the writer Gang, and block to get results or error
	
### Dispatcher routines:
All dispatcher routines contains few standard steps:
* CdbDispatchPlan/CdbDispatchUtilityStatement/CdbDispatchCommand/CdbDispatchSetCommand/CdbDispDtxProtocolCommand
	* `cdbdisp_makeDispatcherState`: create a dispatcher state and register it in the resource owner release callback.
	* `buildGpQueryString/buildGpDtxProtocolCommand`: serialize Plan/Utility/Command to raw text QEs can recognize, must allocate it within DispatcherContex.
	* `AllocateWriterGang/AssignGangs`: allocate a gang or a bunch of gangs (for Plan) and prepare for execution, gangs are tracked by dispatcher state
	* `cdbdisp_dispatchToGang`: send serialized raw query to QEs in unblocking mode which means the data in connection is not guaranteed being flushed, this is very useful if a plan contains multiple slices, so dispatcher don't block when libpq connections is congested
	* `cdbdisp_waitDispatchFinish`: as described above, this function will poll on libpq connections and flush the data in bunches 
	* `cdbdisp_checkDispatchResult`: block until QEs report a command OK response or an error etc
	* `cdbdisp_getDispatchResults`: fetch results from dispatcher state or error data if an error occurs
	* `cdbdisp_destroyDispatcherState`: destroy current dispatcher state and recycle gangs allocated by it.

### CdbComponentDatabases
CdbComponentDatabases is a snapshot of current cluster components based on catalog gp_segment_configuration.
It provides information about each segment component include dbid, contentid, hostname, ip address, current role etc.
It also maintains a pool of idle QEs (SegmentDatabaseDescriptor), dispatcher can reuse those QEs between statements in the same session.

CdbComponentDatabases has a memory context named CdbComponentsContext associated.

#### CdbComponentDatabases routines:
There are a few functions to manipulate CdbComponentDatabases, Dispatcher can use those functions to make up/clean up a gang.

* cdbcomponent_getCdbComponents(): get a snapshot of current cluster components from gp_segment_configuration. When FTS version changed since last time, destroy current components snapshot and get a new one if 1) session has no temp tables 2) current gxact need two-phase commit but gxid has not been dispatched to segments yet or current gxact don't need two phase commit.

* cdbcomponent_destroyCdbComponents(): destroy a snapshot of current cluster components including the MemoryContext and pool of idle QEs for all segments.

* cdbcomponent_allocateIdleQE(contentId, SegmentType): allocate a free QE by 1) reuse a QE from pool of idle QEs. 2) create a brand new QE. For case2, the connection is not actually established inside this function, the caller should establish the connections in a batch to gain performance. This function guarantees each segment in a session have only one writer. SegmentType can be:
	* SEGMENTTYPE_EXPLICT_WRITER : must be writer, for DTX commands and DDL commands and DML commands need two-phase commit.
	* SEGMENTTYPE_EXPLICT_READER : must be reader, only be used by cursor.
	* SEGMENTTYPE_ANY: any type is ok, for most of queries which don't need two-phase commit.

* cdbcomponent_recycleIdleQE(SegmentDatabaseDescriptor): recycle/destroy a QE. a QE will be destroyed if 1) caller specify forceDestroy to true. 2)connection to QE is already bad. 3) FTS detect the segment is already down. 4) exceeded the pool size of idle QEs. 5) cached memory exceeded the limitation. otherwise, the QE will be put into a pool for reusing later.

* cdbcomponent_cleanupIdleQE(includeWriter): disconnect and destroy idle QEs of all segments. includeWriter tells cleanup idle writers or not.
