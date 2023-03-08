# gpmt analyze\_session 

This tool traces busy processes associated with a Greenplum Database session. The information collected can be used by VMware Support for root cause analysis on hung sessions.

## <a id="usage"></a>Usage 

```
gpmt analyze_session [-session <session_id> ] [-master-dir <directory>] 
[-segment-dir <directory>] 
```

## <a id="options"></a>Options 

-session
:   Greenplum session ID which is referenced in `pg_stat_activity`.

-master-dir
:   Working directory for coordinator process.

-segment-dir
:   Working directory for segment processes.

-free-space
:   Free space threshold which will exit log collection if reached. Default value is 10%.

-a
:   Answer Yes to all prompts.

## <a id="examples"></a>Examples 

Collect process information for a given Greenplum Database session id:

```
gpmt analyze_session -session 12345
```

The tool prompt gives a high-level list of only the servers that are running busy processes and how processes are distributed across the Greenplum hosts. This gives an idea of what hosts are busier than others, which might be caused by processing skew or other environmental issue with the affected hosts.

Note: `lsof, strace, pstack, gcore, gdb` must be installed on all hosts. `gcore` will perform a memory dump of the Greenplum process and the size could be anywhere from 300MB to several Gigabytes. Isolating which hosts to collect using the `gpmt` global option `-hostfile` to limit the collection size.

