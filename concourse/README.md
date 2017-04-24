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
* `gpcloud_pipeline.yml` run gpcloud tests against developers specified branch.

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
Use this team to create any pipelines instead of the `main` team.

### Creating Your Own Pipeline
Many developers want to create their own copies of the master pipeline.
To accommodate this without naming confusion, workload instability, and artifact collision we have the following solution:

1. Fork or branch the gpdb git repo
1. Create an s3 bucket for your pipeline to use
1. Edit `concourse/pipelines/pipeline.yml` to point at your git branch and s3 bucket
1. Commit and push your branch
1. Set the pipeline using a unique pipeline name. Example:

```bash
fly -t gpdb login
fly -t gpdb set-pipeline -c concourse/pipelines/pipeline.yml -p NEW_PIPELINE_NAME
```
#### Notes and warnings

* Clean up your dev pipelines when you are finished with them. (Use `fly destroy-pipeline`)
* Be sure to use a unique name for your pipeline. Don't blow away the master pipeline by using `gpdb_master` when setting your dev pipeline.
* Please pause the pulse jobs (the rightmost column of builds). If you are working on one, that's fine, but pause all of the others. We've had issues with too many pulse jobs running at once.

### Updating This README
Changes should be proposed to this contract with a PR.

# Triggering pulse projects
Use `gpdb5-pulse-worker` tag to invoke the job to trigger and monitor Pulse projects 
