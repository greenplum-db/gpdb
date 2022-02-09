---
title: Setting Parameters at the Role Level 
---

Use `ALTER ROLE` to set a parameter at the role level. For example:

```
=# ALTER ROLE bob SET search_path TO bobschema;
```

When you set a session parameter at the role level, every session initiated by that role uses that parameter setting. Settings at the role level override settings at the database level.

**Parent topic:**[Setting a Master Configuration Parameter](../topics/g-setting-a-master-configuration-parameter.html)

