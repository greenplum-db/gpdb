#!/usr/bin/env python3
# ----------------------------------------------------------------------
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
# ----------------------------------------------------------------------

"""Generate pipeline (default: gpdb_main-generated.yml) from template (default:
templates/gpdb-tpl.yml).

Python module requirements:
  - jinja2 (install through pip or easy_install)
"""

from __future__ import print_function

import argparse
import datetime
import os
import re
import subprocess
import yaml

from jinja2 import Environment, FileSystemLoader

PIPELINES_DIR = os.path.dirname(os.path.abspath(__file__))

TEMPLATE_ENVIRONMENT = Environment(
    autoescape=False,
    loader=FileSystemLoader(os.path.join(PIPELINES_DIR, 'templates')),
    trim_blocks=True,
    lstrip_blocks=True,
    variable_start_string='[[',  # 'default {{ has conflict with pipeline syntax'
    variable_end_string=']]',
    extensions=['jinja2.ext.loopcontrols']
)

BASE_BRANCH = "main"  # when branching gpdb update to 7X_STABLE, 6X_STABLE, etc.

CI_VARS_PATH = os.path.join(os.getcwd(), '..', 'vars')

# Variables that govern pipeline validation
RELEASE_VALIDATOR_JOB = ['Release_Candidate', 'Build_Release_Candidate_RPMs']
JOBS_THAT_ARE_GATES = [
    'gate_icw_end',
    'gate_resource_groups_start',
    'gate_cli_start',
    'gate_release_candidate_start'
]

JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE = (
    [
        'combine_cli_coverage',
        'compile_gpdb_clients_windows',
        'Publish Server Builds',
    ] + RELEASE_VALIDATOR_JOB + JOBS_THAT_ARE_GATES
)

default_os_type = 'rocky8'

def suggested_git_remote():
    """Try to guess the current git remote"""
    default_remote = "<https://github.com/<github-user>/gpdb>"

    remote = subprocess.check_output(["git", "ls-remote", "--get-url"]).decode('utf-8').rstrip()

    if "greenplum-db/gpdb" in remote:
        return default_remote

    if "git@" in remote:
        git_uri = remote.split('@')[1]
        hostname, path = git_uri.split(':')
        return 'https://%s/%s' % (hostname, path)

    return remote


def suggested_git_branch():
    """Try to guess the current git branch"""
    branch = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"]).decode('utf-8').rstrip()
    if branch == "main" or is_a_base_branch(branch):
        return "<branch-name>"
    return branch


def is_a_base_branch(branch):
    # best effort in matching a base branch (5X_STABLE, 6X_STABLE, etc.)
    matched = re.match("\d+X_STABLE", branch)
    return matched is not None


def render_template(template_filename, context):
    """Render pipeline template yaml"""
    return TEMPLATE_ENVIRONMENT.get_template(template_filename).render(context)


def validate_pipeline_release_jobs(raw_pipeline_yml, os_type):
    """Make sure all jobs in specified pipeline that don't block release are accounted
    for (they should belong to JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE, defined above)"""
    print("======================================================================")
    print("Validate Pipeline Release Jobs")
    print("----------------------------------------------------------------------")

    # ignore concourse v2.x variable interpolation
    pipeline_yml_cleaned = re.sub('{{', '', re.sub('}}', '', raw_pipeline_yml))
    pipeline = yaml.safe_load(pipeline_yml_cleaned)

    jobs_raw = pipeline['jobs']
    all_job_names = [job['name'] for job in jobs_raw]

    if os_type != "rhel8" and os_type != "oel8":
        rc_name = 'gate_release_candidate_start'
        release_candidate_job = [j for j in jobs_raw if j['name'] == rc_name][0]

        release_blocking_jobs = release_candidate_job['plan'][0]['in_parallel']['steps'][0]['passed']

        non_release_blocking_jobs = [j for j in all_job_names if j not in release_blocking_jobs]

        unaccounted_for_jobs = \
            [j for j in non_release_blocking_jobs if j not in JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE]

        if unaccounted_for_jobs:
            print("Please add the following jobs as a Release_Candidate dependency or ignore them")
            print("by adding them to JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE in " + __file__)
            print(unaccounted_for_jobs)
            return False

    print("Pipeline validated: all jobs accounted for")
    return True


def create_pipeline(args, git_remote, git_branch):
    """Generate OS specific pipeline sections"""
    if args.test_trigger_false:
        test_trigger = "true"
    else:
        test_trigger = "false"

    variables_type = args.pipeline_target
    os_username = {
        "rhel8" : "rhel",
        "rocky8" : "rocky",
        "oel8" : "oel"
    }
    test_os = {
        "rhel8" : "centos",
        "rocky8": "centos",
        "oel8" : "centos"
    }


    context = {
        'template_filename': args.template_filename,
        'generator_filename': os.path.basename(__file__),
        'timestamp': datetime.datetime.now(),
        'os_type': args.os_type,
        'default_os_type': default_os_type,
        'os_username': os_username[args.os_type],
        'test_os': test_os[args.os_type],
        'pipeline_target': args.pipeline_target,
        'test_sections': args.test_sections,
        'pipeline_configuration': args.pipeline_configuration,
        'test_trigger': test_trigger,
        'use_ICW_workers': args.use_ICW_workers,
        'build_test_rc_rpm': args.build_test_rc_rpm,
        'directed_release': args.directed_release,
        'git_username': git_remote.split('/')[-2],
        'git_branch': git_branch,
        'variables_type': variables_type
    }

    pipeline_yml = render_template(args.template_filename, context)
    if args.pipeline_target == 'prod':
        validated = validate_pipeline_release_jobs(pipeline_yml, args.os_type)
        if not validated:
            print("Refusing to update the pipeline file")
            return False

    with open(args.output_filepath, 'w') as output:
        header = render_template('pipeline_header.yml', context)
        output.write(header)
        output.write(pipeline_yml)

    return True


def gen_pipeline(args, pipeline_name, variable_files, git_remote, git_branch):
    variables = ""
    for variable in variable_files:
        variables += "-l %s/%s " % (CI_VARS_PATH, variable)

    format_args = {
        'target': args.pipeline_target,
        'name': pipeline_name,
        'output_path': args.output_filepath,
        'variables': variables,
        'remote': git_remote,
        'branch': git_branch,
    }

    return '''fly --target {target} \
set-pipeline \
--check-creds \
--pipeline {name} \
--config {output_path} \
{variables} \
--var gpdb-git-remote={remote} \
--var gpdb-git-branch={branch} \
--var pipeline-name={name} \

'''.format(**format_args)


def header(args):
    return '''
======================================================================
  Pipeline target: ......... : %s
  Pipeline file ............ : %s
  Template file ............ : %s
  OS Type .................. : %s
  Test sections ............ : %s
  test_trigger ............. : %s
  use_ICW_workers .......... : %s
  build_test_rc_rpm ........ : %s
  directed_release ......... : %s
======================================================================
''' % (args.pipeline_target,
       args.output_filepath,
       args.template_filename,
       args.os_type,
       args.test_sections,
       args.test_trigger_false,
       args.use_ICW_workers,
       args.build_test_rc_rpm,
       args.directed_release
       )


def print_fly_commands(args, git_remote, git_branch):
    pipeline_name = os.path.basename(args.output_filepath).rsplit('.', 1)[0]

    print(header(args))
    if args.directed_release: 
        print('NOTE: You can set the directed release pipeline with the following:\n')
        print(gen_pipeline(args, pipeline_name, ["common_prod.yml", "without_asserts_common_prod.yml"],
                           "https://github.com/greenplum-db/gpdb.git", git_branch))
        return
    if args.pipeline_target == 'prod':
        print('NOTE: You can set the production pipelines with the following:\n')
        pipeline_name = "gpdb_%s" % BASE_BRANCH if BASE_BRANCH == "main" else BASE_BRANCH
        if args.os_type != default_os_type:
            pipeline_name += "_" + args.os_type
        print(gen_pipeline(args, pipeline_name, ["common_prod.yml"],
                           "https://github.com/greenplum-db/gpdb.git", BASE_BRANCH))
        print(gen_pipeline(args, "%s_without_asserts" % pipeline_name, ["common_prod.yml", "without_asserts_common_prod.yml"],
                           "https://github.com/greenplum-db/gpdb.git", BASE_BRANCH))
        return

    else:
        print('NOTE: You can set the developer pipeline with the following:\n')
        print(gen_pipeline(args, pipeline_name, ["common_prod.yml", "common_" + args.pipeline_target + ".yml"], git_remote, git_branch))

def main():
    """main: parse args and create pipeline"""
    parser = argparse.ArgumentParser(
        description='Generate Concourse Pipeline utility',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument(
        '-T',
        '--template',
        action='store',
        dest='template_filename',
        default="gpdb-tpl.yml",
        help='Name of template to use, in templates/'
    )

    default_output_filename = "gpdb_%s-generated.yml" % BASE_BRANCH
    parser.add_argument(
        '-o',
        '--output',
        action='store',
        dest='output_filepath',
        default=os.path.join(PIPELINES_DIR, default_output_filename),
        help='Output filepath to use for pipeline file, and from which to derive the pipeline name.'
    )

    parser.add_argument(
        '-O',
        '--os_type',
        action='store',
        dest='os_type',
        default=default_os_type,
        choices=['rhel8', 'rocky8', 'oel8'],
        help='OS value to support'
    )

    parser.add_argument(
        '-t',
        '--pipeline_target',
        action='store',
        dest='pipeline_target',
        default='dev',
        help='Concourse target supported: prod, dev, dev2, cm, ud, or dp. '
             'The Pipeline target value is also used to identify the CI '
             'project specific common file in the vars directory.'
    )

    parser.add_argument(
        '-c',
        '--configuration',
        action='store',
        dest='pipeline_configuration',
        default='default',
        help='Set of platforms and test sections to use; only works with dev and team targets, ignored with the prod target.'
             'Valid options are prod (same as the prod pipeline), full (everything except release jobs), and default '
             '(follow the -A and -O flags).'
    )

    parser.add_argument(
        '-a',
        '--test_sections',
        action='store',
        dest='test_sections',
        choices=[
            'ICW',
            'ResourceGroups',
            'Interconnect',
            'CLI',
            'Extensions'
        ],
        default=[],
        nargs='+',
        help='Select tests sections to run'
    )

    parser.add_argument(
        '-n',
        '--test_trigger_false',
        action='store_false',
        default=True,
        help='Set test triggers to "false". This only applies to dev pipelines.'
    )

    parser.add_argument(
        '-u',
        '--user',
        action='store',
        dest='user',
        default=os.getlogin(),
        help='Developer userid to use for pipeline name and filename.'
    )

    parser.add_argument(
        '-U',
        '--use_ICW_workers',
        action='store_true',
        default=False,
        help='Set use_ICW_workers to "true".'
    )

    parser.add_argument(
        '--build-test-rc',
        action='store_true',
        dest='build_test_rc_rpm',
        default=False,
        help='Generate a release candidate RPM. Useful for testing branches against'
             'products that consume RC RPMs such as gpupgrade. Use prod'
             'configuration to build prod RCs.'
    )

    parser.add_argument(
        '--directed',
        action='store_true',
        dest='directed_release',
        default=False,
        help='Generates a pipeline for directed releases. '
             'This flag can be used only with the prod target.'
    )

    args = parser.parse_args()

    if args.pipeline_target == 'prod' and args.build_test_rc_rpm:
        raise Exception('Cannot specify a prod pipeline when building a test'
                        'RC. Please specify one or the other.')

    if args.pipeline_target != 'prod' and args.directed_release:
        raise Exception('--directed flag can be used only with prod target')

    output_path_is_set = os.path.basename(args.output_filepath) != default_output_filename
    if (args.user != os.getlogin() and output_path_is_set):
        print("You can only use one of --output or --user.")
        exit(1)

    if args.pipeline_target == 'prod' and not args.directed_release:
        args.pipeline_configuration = 'prod'

    # use_ICW_workers adds tags to the specified concourse definitions which
    # correspond to dedicated concourse workers to increase performance.
    if args.pipeline_target in ['prod', 'dev', 'cm']:
        args.use_ICW_workers = True

    if args.pipeline_configuration == 'prod' or args.pipeline_configuration == 'full' or args.directed_release:
        args.test_sections = [
            'ICW',
            'ResourceGroups',
            'Interconnect',
            'CLI',
            'Extensions'
        ]

    git_remote = suggested_git_remote()
    git_branch = suggested_git_branch()

    # if generating a dev pipeline but didn't specify an output,
    # don't overwrite the main pipeline
    if args.pipeline_target != 'prod' and not output_path_is_set:
        pipeline_file_suffix = suggested_git_branch()
        if args.user != os.getlogin():
            pipeline_file_suffix = args.user
        default_dev_output_filename = 'gpdb-' + args.pipeline_target + '-' + pipeline_file_suffix + '-' + args.os_type + '.yml'
        args.output_filepath = os.path.join(PIPELINES_DIR, default_dev_output_filename)

    if args.directed_release:
        pipeline_file_suffix = suggested_git_branch()
        default_dev_output_filename = pipeline_file_suffix + '.yml'
        args.output_filepath = os.path.join(PIPELINES_DIR, default_dev_output_filename)

    pipeline_created = create_pipeline(args, git_remote, git_branch)

    if not pipeline_created:
        exit(1)

    print_fly_commands(args, git_remote, git_branch)


if __name__ == "__main__":
    main()
