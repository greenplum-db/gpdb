---
title: About the Greenplum Database Release Version Number 
---

Greenplum Database version numbers and the way they change identify what has been modified from one Greenplum Database release to the next.

A Greenplum Database release version number takes the format x.y.z, where:

-   x identifies the Major version number
-   y identifies the Minor version number
-   z identifies the Patch version number

Greenplum Database releases that have the same Major release number are guaranteed to be backwards compatible. Greenplum Database increments the Major release number when the catalog changes or when incompatible feature changes or new features are introduced. Previously deprecated functionality may be removed in a major release.

The Minor release number for a given Major release increments when backwards compatible new features are introduced or when a Greenplum Database feature is deprecated. \(Previously deprecated functionality will never be removed in a minor release.\)

Greenplum Database increments the Patch release number for a given Minor release for backwards-compatible bug fixes.

**Parent topic:** [Managing a Greenplum System](../managing/partII.html)

