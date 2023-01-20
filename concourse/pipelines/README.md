# Concourse Pipeline Generation

To facilitate pipeline maintenance, a Python utility `gen_pipeline.py`
is used to generate the production pipeline. It can also be used to build
custom pipelines for developer use.

The utility uses the [Jinja2](http://jinja.pocoo.org/) template
engine for Python. This allows the generation of portions of the
pipeline from common blocks of pipeline code. Logic (Python code) can
be embedded to further manipulate the generated pipeline.

You can think of the usage of this utility, supporting template and
generated pipeline file in similar terms to the autoconf, configure.in and
configure workflow.

## IMPORTANT

* Under no circumstances should any credentials be committed into
  public repos.
* The production pipeline should not be edited directly. Only the
  editing of the template should be performed.
* The utility may recommend setting more than one production pipeline.

## Workflow

The following workflow should be followed:

* Edit the template file (`templates/gpdb-tpl.yml`).
* Generate the pipelines. During this step, the pipeline release jobs will be validated.
* Use the Concourse `fly` command to set the pipeline (`gpdb_main-generated.yml`).
* Once the pipeline is validated to function properly, commit the updated template and pipeline.

## Requirements

### [Jinja2](http://jinja.pocoo.org/)
[Jinja2](http://jinja.pocoo.org/) can be readily installed using `easy_install` or `pip`.

You can install the most recent Jinja2 version using easy_install or pip:

```
easy_install Jinja2
pip3 install Jinja2
```

For in-depth information, please refer to it's documentation.

## Usage

### Help
The utility options can be retrieved with the following:
```
gen_pipeline.py -h|--help
```

## Examples of usage

### Create and Update Production Pipelines

The `./gen_pipeline.py -t prod` command will generate the production
pipeline (`gpdb_main-generated.yml`). Default platform and
test sections are included. The pipeline release jobs will be
validated. The output of the utility will provide details of the
pipeline generated. Following standard conventions, two `fly`
commands are provided as output so the engineer can copy and
paste this into their terminal to set the production pipelines.

Example:

```
$ ./gen_pipeline.py -t prod
======================================================================
Validate Pipeline Release Jobs
----------------------------------------------------------------------
Pipeline validated: all jobs accounted for

======================================================================
  Pipeline target: ......... : prod
  Pipeline file ............ : gpdb_main-generated.yml
  Template file ............ : gpdb-tpl.yml
  OS Type .................. : rocky8
  Test sections ............ : ['ICW', 'ResourceGroups', 'Interconnect', 'CLI', 'Extensions']
  test_trigger ............. : True
  use_ICW_workers .......... : True
  build_test_rc_rpm ........ : False
  directed_release ......... : False
======================================================================

NOTE: You can set the production pipelines with the following:

fly -t prod \
    set-pipeline \
    -p gpdb_main \
    -c gpdb_main-generated.yml \
    -l ~/workspace/gpdb/concourse/vars/common_prod.yml \
    -v gpdb-git-remote=https://github.com/greenplum-db/gpdb.git \
    -v gpdb-git-branch=main \
    -v pipeline-name=gpdb_main

fly -t prod \
    set-pipeline \
    -p gpdb_main_without_asserts \
    -c gpdb_main-generated.yml \
    -l ~/workspace/gpdb/concourse/vars/common_prod.yml \
    -l ~/workspace/gpdb/concourse/vars/without_asserts_common_prod.yml \
    -v gpdb-git-remote=https://github.com/greenplum-db/gpdb.git \
    -v gpdb-git-branch=main \
    -v pipeline-name=gpdb_main_without_asserts
```

The generated pipeline file `gpdb_main-generated.yml` will be set,
validated and ultimately committed (including the updated pipeline
template) to the source repository.

Create and update gpdb_main_rhel8, gpdb_main_oel8 pipelines:

```
./gen_pipeline.py  -t prod -O rhel8
./gen_pipeline.py  -t prod -O oel8
```



### Creating Developer pipelines

As an example of generating a pipeline with a targeted test subset,
the following can be used to generate a pipeline with supporting
builds (default: rhel8 platform) and `CLI` only jobs.

The generated pipeline and helper `fly` command are intended encourage
engineers to set the pipeline with a team-name-string (-t) and engineer
(-u) identifiable names.

NOTE: The majority of jobs are only available for the `rhel8`
      platform.

```
$ ./gen_pipeline.py -t dpm -u curry -a CLI

======================================================================
  Generate Pipeline type: .. : dpm
  Pipeline file ............ : gpdb-dpm-curry.yml
  Template file ............ : gpdb-tpl.yml
  OS Types ................. : ['rhel8']
  Test sections ............ : ['CLI']
  test_trigger ............. : True
======================================================================

NOTE: You can set the developer pipeline with the following:

fly -t dev \
    set-pipeline \
    -p gpdb-dpm-curry \
    -c gpdb-dpm-curry.yml \
    -l ~/workspace/gpdb/concourse/vars/common_prod.yml \
    -l ~/workspace/gpdb/concourse/vars/common_dev.yml \
    -v gpdb-git-remote=<https://github.com/<github-user>/gpdb> \
    -v gpdb-git-branch=<branch-name>
```

Use the following to generate a pipeline with `ICW` and `CS` test jobs
for `rhel8` platforms.

```
$ ./gen_pipeline.py -t cs -u durant -O {rhel8} -a {ICW,CS}

======================================================================
  Generate Pipeline type: .. : cs
  Pipeline file ............ : gpdb-cs-durant.yml
  Template file ............ : gpdb-tpl.yml
  OS Types ................. : ['rhel8']
  Test sections ............ : ['ICW', 'CS']
  test_trigger ............. : True
======================================================================

NOTE: You can set the developer pipeline with the following:

fly -t dev \
    set-pipeline \
    -p gpdb-cs-durant \
    -c gpdb-cs-durant.yml \
    -l ~/workspace/gpdb/concourse/vars/common_prod.yml \
    -l ~/workspace/gpdb/concourse/vars/common_dev.yml \
    -v gpdb-git-remote=<https://github.com/<github-user>/gpdb> \
    -v gpdb-git-branch=<branch-name>
```
