---
title: Viewing Server Configuration Parameter Settings 
---

The SQL command `SHOW` allows you to see the current server configuration parameter settings. For example, to see the settings for all parameters:

```
$ psql -c 'SHOW ALL;'
```

`SHOW` lists the settings for the coordinator instance only. To see the value of a particular parameter across the entire system \( and all segments\), use the `gpconfig` utility. For example:

```
$ gpconfig --show max_connections
```

**Parent topic:** [Configuring the Greenplum Database System](../topics/g-configuring-the-greenplum-system.html)

