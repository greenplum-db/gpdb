# Concourse Pipeline Generation

To facilitate pipeline maintanance, a Python utility 'gen_pipeline.py`
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
* Generate the pipeline. During this step, the pipeline release jobs will be validated.
* Use the Concourse `fly` command to set the pipeline (`gpdb_master-generated.yml`).
* Once the pipeline is validated to function properly, commit the updated template and pipeline.

## Requirements

### [Jinja2](http://jinja.pocoo.org/)
[Jinja2](http://jinja.pocoo.org/) can be readily installed using `easy_install` or `pip`.

You can install the most recent Jinja2 version using easy_install or pip:

```
easy_install Jinja2
pip install Jinja2
```

For in-depth information, please refer to it's documentation.

## Usage

### Help
The utility options can be retrieved with the following:
```
gen_pipeline.py -h|--help
```

## Examples of usage

### Create Production Pipeline

The `./gen_pipeline.py -t prod` command will generate the production
pipeline (`gpdb_master-generated.yml`). All supported platforms and
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
all jobs accounted for

======================================================================
  Generate Pipeline type: .. : prod
  Pipeline file ............ : gpdb_master-generated.yml
  Template file ............ : gpdb-tpl.yml
  OS Types ................. : ['centos6', 'centos7', 'sles', 'aix7', 'win', 'ubuntu16']
  Test sections ............ : ['ICW', 'CS', 'MPP', 'MM', 'DPM', 'UD', 'FileRep']
  test_trigger ............. : True
======================================================================

NOTE: You can set the production pipelines with the following:

fly -t gpdb-prod \
    set-pipeline \
    -p gpdb_master \
    -c gpdb_master-generated.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_common-ci-secrets.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_master-ci-secrets.yml

fly -t gpdb-prod \
    set-pipeline \
    -p gpdb_master_without_asserts \
    -c gpdb_master-generated.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_common-ci-secrets.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_master_without_asserts-ci-secrets.yml
```

The generated pipeline file `gpdb_master-generated.yml` will be set,
validated and ultimately committed (including the updated pipeline
template) to the source repository.

### Creating Developer pipelines

As an example of generating a pipeline with a targeted test subset,
the following can be used to generate a pipeline with supporting
builds (default: centos6 platform) and `DPM` only jobs.

The generated pipeline and helper `fly` command are intended encourage
engineers to set the pipeline with a team-name-string (-t) and engineer
(-u) identifiable names.

NOTE: The majority of jobs are only available for the `centos6`
      platform.

```
$ ./gen_pipeline.py -t dpm -u curry -a DPM

======================================================================
  Generate Pipeline type: .. : dpm
  Pipeline file ............ : gpdb-dpm-curry.yml
  Template file ............ : gpdb-tpl.yml
  OS Types ................. : ['centos6']
  Test sections ............ : ['DPM']
  test_trigger ............. : True
======================================================================

NOTE: You can set the developer pipeline with the following:

fly -t gpdb-dev \
    set-pipeline \
    -p gpdb-dpm-curry \
    -c gpdb-dpm-curry.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_common-ci-secrets.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_master-ci-secrets.yml \
    -v tf-bucket-path=dev/dpm/ \
    -v bucket-name=gpdb5-concourse-builds-dev
```

Use the following to generate a pipeline with `ICW` and `CS` test jobs
for `centos6` and `sles` platforms.

```
$ ./gen_pipeline.py -t cs -u durant -O {centos6,sles} -a {ICW,CS}

======================================================================
  Generate Pipeline type: .. : cs
  Pipeline file ............ : gpdb-cs-durant.yml
  Template file ............ : gpdb-tpl.yml
  OS Types ................. : ['centos6', 'sles']
  Test sections ............ : ['ICW', 'CS']
  test_trigger ............. : True
======================================================================

NOTE: You can set the developer pipeline with the following:

fly -t gpdb-dev \
    set-pipeline \
    -p gpdb-cs-durant \
    -c gpdb-cs-durant.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_common-ci-secrets.yml \
    -l ~/workspace/continuous-integration/secrets/gpdb_master-ci-secrets.yml \
    -v tf-bucket-path=dev/cs/ \
    -v bucket-name=gpdb5-concourse-builds-dev

```
