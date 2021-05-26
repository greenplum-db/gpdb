# pg_upgrade tests

pg_upgrade tests have been moved to the [gpupgrade repo](https://github.com/greenplum-db/gpupgrade) and associated [pipeline](https://prod.ci.gpdb.pivotal.io/teams/main/pipelines/gpupgrade). 
To add tests see the [gpupgrade/test/README.md](https://github.com/greenplum-db/gpupgrade/blob/master/test/README.md).

The motivation for moving the tests:
- The pg_upgrade tests require upgrading an entire cluster and thus instead of duplicating that logic simply use gpupgrade.
This improves clarity and avoids code divergence.
- Consolidating all tests in the gpupgrade repo allows the test framework to run against multiple GPDB source and target 
versions such as 5->6, and 6->7.
