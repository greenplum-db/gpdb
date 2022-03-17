---
title: Setting Parameters at the Database Level 
---

Use `ALTER DATABASE` to set parameters at the database level. For example:

```
=# ALTER DATABASE mydatabase SET search_path TO myschema;
```

When you set a session parameter at the database level, every session that connects to that database uses that parameter setting. Settings at the database level override settings at the system level.

**Parent topic:**[Setting a Master Configuration Parameter](../topics/g-setting-a-master-configuration-parameter.html)

