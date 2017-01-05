# Concourse Directory Contract

### Directory Structure
The Concourse directory should contain this README and three sub-directories only:

* pipelines
* tasks
* scripts

##### Pipelines Directory
There should be seven pipelines in this directory:

* `pipeline.yml` the pipeline that compiles, tests, and produces installers from the master branch of gpdb.
* `dev_generate_installer.yml` which compiles and generates an installer for the given source and saves it to a dev bucket.
* `pr_pipeline.yml` which compiles and tests pull requests.
* `concourse-upgrade.yml` which uses Concourse to upgrade itself.
* `pipeline_tinc.yml` run TINC tests against gpdb master branch.
* `pipeline_gpcloud.yml` run gpcloud tests against developers specified branch.

##### Tasks Directory
All task yamls should live in this directory.
If a task file is not referenced in the `pipelines` directory it is considered abandoned and can be removed.

There are some exceptions to this rule.
Please do not create any more exceptions, and remove these as the occasion arises:

* `package_tarball.yml` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `test_with_planner.yml` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `test_with_orca.yml` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `build_with_orca.yml` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `gpcheckcloud_tests_gpcloud.yml` is being used by [a gpcloud pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline_gpcloud.yml)
* `regression_tests_gpcloud.yml` is being used by the main pipeline and [a gpcloud pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline_gpcloud.yml)
* `unit_tests_gpcloud.yml` is being used by [a gpcloud pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline_gpcloud.yml)

##### Scripts Directory
All script files should live in this directory.
If a script file is not referenced in any of the directories it is considered abandoned and can be removed.

There are some exceptions to this rule.
Please do not create any more exceptions, and remove these as the occasion arises:

* `cpplint.py` is being used by codegen
* `package_tarball.bash` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `builds/` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `test_gpdb.py` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `build_gpdb.py` is being used by [a gporca pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/pipeline.yml)
* `gpcheckcloud_tests_gpcloud.bash` is being used by [a gpcloud pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/tasks/gpcheckcloud_test_gpcloud.yml)
* `regression_tests_gpcloud.bash` is being used by [a gpcloud pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/tasks/regression_tests_gpcloud.yml)
* `unit_tests_gpcloud.bash` is being used by [a gpcloud pipeline ](https://github.com/greenplum-db/gporca/blob/master/concourse/tasks/unit_tests_gpcloud.yml)

### The Concourse Deployment
There is a `gpdb` team in the [Concourse instance](http://gpdb.ci.pivotalci.info/).
This team should only have the `gpdb_master`, `gpdb_pr`, `gpdb_master_tinc_native`, and `concourse_upgrade` pipelines.

There is a `dev` team which is where developer pipelines should live.
This team should have one permanent template pipeline `gpdb_master_dev` which should remain paused.

### Creating Your Own Pipeline
Many developers want to create their own copies of the master pipeline.
To accommodate this without naming confusion, workload instability, and artifact collision we have the following solution:

1. All developer pipelines should live in the `dev` team.
1. Duplicate and update the `gpdb_master_dev` so that the artifacts are placed in a safe bucket.

To duplicate the pipeline do something like the following:

```bash
fly -t gpdb login -n dev
fly -t gpdb get-pipeline -p gpdb_master_dev > /tmp/pipeline.yml
# update gpdb_src with your desired source
fly -t gpdb set-pipeline -c /tmp/pipeline.yml -p NEW_PIPELINE_NAME
```

### Updating This README
Changes should be proposed to this contract with a PR.

# Triggering pulse projects
Use `gpdb5-pulse-worker` tag to invoke the job to trigger and monitor Pulse projects 
