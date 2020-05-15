#!/usr/bin/env python
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

"""Generate pipeline (default: gpdb_master-generated.yml) from template (default:
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

BASE_BRANCH = "master"  # when branching gpdb update to 7X_STABLE, 6X_STABLE, etc.

SECRETS_PATH = os.path.expanduser('~/workspace/gp-continuous-integration/secrets')

# Variables that govern pipeline validation
RELEASE_VALIDATOR_JOB = ['Release_Candidate']
JOBS_THAT_ARE_GATES = [
    'gate_icw_start',
    'gate_icw_end',
    'gate_replication_start',
    'gate_resource_groups_start',
    'gate_cli_start',
    'gate_ud_start',
    'gate_advanced_analytics_start',
    'gate_release_candidate_start'
]

JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE = (
    [
        'combine_cli_coverage',
        'compile_gpdb_binary_swap_centos7',
        'compile_gpdb_clients_windows',
        'concourse_unit_tests',
        'test_gpdb_clients_windows',
        'walrep_2',
        'madlib_build_gppkg',
        'MADlib_Test_planner_centos7',
        'MADlib_Test_orca_centos7',
        'Publish Server Builds',
    ] + RELEASE_VALIDATOR_JOB + JOBS_THAT_ARE_GATES
)


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
    if branch == "master" or is_a_base_branch(branch):
        return "<branch-name>"
    return branch


def is_a_base_branch(branch):
    # best effort in matching a base branch (5X_STABLE, 6X_STABLE, etc.)
    matched = re.match("\d+X_STABLE", branch)
    return matched is not None


def render_template(template_filename, context):
    """Render pipeline template yaml"""
    return TEMPLATE_ENVIRONMENT.get_template(template_filename).render(context)


def validate_pipeline_release_jobs(raw_pipeline_yml):
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


def validate_target(target):
    expected_secrets_file = "%s/ccp_ci_secrets_%s.yml" % (SECRETS_PATH, target)

    if not os.path.exists(expected_secrets_file):
        raise Exception('Invalid target "%s"; no secrets file found.  Please ensure your secrets files in %s are up to date.' % (target, SECRETS_PATH))


def create_pipeline(args):
    """Generate OS specific pipeline sections"""
    if args.test_trigger_false:
        test_trigger = "true"
    else:
        test_trigger = "false"

    context = {
        'template_filename': args.template_filename,
        'generator_filename': os.path.basename(__file__),
        'timestamp': datetime.datetime.now(),
        'os_types': args.os_types,
        'test_sections': args.test_sections,
        'pipeline_configuration': args.pipeline_configuration,
        'test_trigger': test_trigger
    }

    pipeline_yml = render_template(args.template_filename, context)
    if args.pipeline_target == 'prod':
        validated = validate_pipeline_release_jobs(pipeline_yml)
        if not validated:
            print("Refusing to update the pipeline file")
            return False

    with open(args.output_filepath, 'w') as output:
        header = render_template('pipeline_header.yml', context)
        output.write(header)
        output.write(pipeline_yml)

    return True


def gen_pipeline(args, pipeline_name, secret_files,
                 git_remote=None,
                 git_branch=None):

    if git_remote is None:
        git_remote = suggested_git_remote()
    if git_branch is None:
        git_branch = suggested_git_branch()

    secrets = ""
    for secret in secret_files:
        secrets += "-l %s/%s " % (SECRETS_PATH, secret)

    format_args = {
        'target': args.pipeline_target,
        'name': pipeline_name,
        'output_path': args.output_filepath,
        'secrets_path': SECRETS_PATH,
        'secrets': secrets,
        'remote': git_remote,
        'branch': git_branch,
    }

    return '''fly -t {target} \
set-pipeline \
-p {name} \
-c {output_path} \
-l {secrets_path}/gpdb_common-ci-secrets.yml \
{secrets} \
-v gpdb-git-remote={remote} \
-v gpdb-git-branch={branch} \
-v pipeline-name={name} \

'''.format(**format_args)


def header(args):
    return '''
======================================================================
  Pipeline target: ......... : %s
  Pipeline file ............ : %s
  Template file ............ : %s
  OS Types ................. : %s
  Test sections ............ : %s
  test_trigger ............. : %s
======================================================================
''' % (args.pipeline_target,
       args.output_filepath,
       args.template_filename,
       args.os_types,
       args.test_sections,
       args.test_trigger_false
       )


def print_fly_commands(args):
    pipeline_name = os.path.basename(args.output_filepath).rsplit('.', 1)[0]

    print(header(args))
    if args.pipeline_target == 'prod':
        print('NOTE: You can set the production pipelines with the following:\n')
        pipeline_name = "gpdb_%s" % BASE_BRANCH if BASE_BRANCH == "master" else BASE_BRANCH
        print(gen_pipeline(args, pipeline_name, ["gpdb_%s-ci-secrets.prod.yml" % BASE_BRANCH],
                           "https://github.com/greenplum-db/gpdb.git", BASE_BRANCH))
        print(gen_pipeline(args, "%s_without_asserts" % pipeline_name, ["gpdb_%s_without_asserts-ci-secrets.prod.yml" % BASE_BRANCH],
                           "https://github.com/greenplum-db/gpdb.git", BASE_BRANCH))
        return

    print('NOTE: You can set the developer pipeline with the following:\n')
    print(gen_pipeline(args, pipeline_name, ["gpdb_%s-ci-secrets.dev.yml" % BASE_BRANCH,
                                             "ccp_ci_secrets_%s.yml" % args.pipeline_target]))


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
        '--os_types',
        action='store',
        dest='os_types',
        default=['centos7'],
        choices=['centos7', 'ubuntu18.04', 'win'],
        nargs='+',
        help='List of OS values to support'
    )

    parser.add_argument(
        '-t',
        '--pipeline_target',
        action='store',
        dest='pipeline_target',
        default='dev',
        help='Concourse target to use either: prod, dev, or <team abbreviation> '
             'where abbreviation is found from the team\'s ccp secrets file name ending.'
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
            'Replication',
            'ResourceGroups',
            'Interconnect',
            'CLI',
            'UD',
            'AA',
            'Extensions'
        ],
        default=['ICW'],
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

    args = parser.parse_args()

    validate_target(args.pipeline_target)

    output_path_is_set = os.path.basename(args.output_filepath) != default_output_filename
    if (args.user != os.getlogin() and output_path_is_set):
        print("You can only use one of --output or --user.")
        exit(1)

    if args.pipeline_target == 'prod':
        args.pipeline_configuration = 'prod'

    if args.pipeline_configuration == 'prod' or args.pipeline_configuration == 'full':
        args.os_types = ['centos6', 'centos7', 'ubuntu18.04', 'win']
        args.test_sections = [
            'ICW',
            'Replication',
            'ResourceGroups',
            'Interconnect',
            'CLI',
            'UD',
            'Extensions'
        ]

    # if generating a dev pipeline but didn't specify an output,
    # don't overwrite the master pipeline
    if args.pipeline_target != 'prod' and not output_path_is_set:
        pipeline_file_suffix = suggested_git_branch()
        if args.user != os.getlogin():
            pipeline_file_suffix = args.user
        default_dev_output_filename = 'gpdb-' + args.pipeline_target + '-' + pipeline_file_suffix + '.yml'
        args.output_filepath = os.path.join(PIPELINES_DIR, default_dev_output_filename)

    pipeline_created = create_pipeline(args)

    if not pipeline_created:
        exit(1)

    print_fly_commands(args)


if __name__ == "__main__":
    main()
