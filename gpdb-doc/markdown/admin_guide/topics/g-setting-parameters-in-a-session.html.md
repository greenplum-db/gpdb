---
title: Setting Parameters in a Session 
---

Any session parameter can be set in an active database session using the `SET` command. For example:

```
=# SET statement_mem TO '200MB';
```

The parameter setting is valid for the rest of that session or until you issue a `RESET` command. For example:

```
=# RESET statement_mem;
```

Settings at the session level override those at the role level.

**Parent topic:**[Setting a Master Configuration Parameter](../topics/g-setting-a-master-configuration-parameter.html)

