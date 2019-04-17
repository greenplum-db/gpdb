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
    variable_start_string='[[', # 'default {{ has conflict with pipeline syntax'
    variable_end_string=']]',
    extensions=['jinja2.ext.loopcontrols']
)

# Variables that govern pipeline validation
RELEASE_VALIDATOR_JOB = ['Release_Candidate']
JOBS_THAT_ARE_GATES = [
    'gate_icw_start',
    'gate_icw_end',
    'gate_replication_start',
    'gate_resource_groups_start',
    'gate_gpperfmon_start',
    'gate_cli_start',
    'gate_ud_start',
    'gate_advanced_analytics_start',
    'gate_release_candidate_start'
]

JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE = (
    [
        'compile_gpdb_binary_swap_centos6',
        'icw_gporca_centos6_gpos_memory',
        'walrep_2',
        'compile_gpdb_sles11',
        'compile_gpdb_ubuntu16',
        'icw_gporca_sles11',
        'icw_gporca_sles12',
        'icw_planner_sles12',
        'icw_planner_ubuntu16',
        'icw_gporca_conan_ubuntu16',
        'gpdb_packaging_ubuntu16',
        'resource_group_sles12',
        'madlib_build_gppkg',
        'MADlib_Test_planner_centos6',
        'MADlib_Test_orca_centos6',
        'MADlib_Test_planner_centos7',
        'MADlib_Test_orca_centos7',
        'icw_extensions_gpcloud_ubuntu16'
    ] + RELEASE_VALIDATOR_JOB + JOBS_THAT_ARE_GATES
)

def suggested_git_remote():
    """Try to guess the current git remote"""
    default_remote = "<https://github.com/<github-user>/gpdb>"

    remote = subprocess.check_output("git ls-remote --get-url", shell=True).rstrip()

    if "greenplum-db/gpdb"  in remote:
        return default_remote

    if "git@" in remote:
        git_uri = remote.split('@')[1]
        hostname, path = git_uri.split(':')
        return 'https://%s/%s' % (hostname, path)

    return remote

def suggested_git_branch():
    """Try to guess the current git branch"""
    default_branch = "<branch-name>"

    branch = subprocess.check_output("git rev-parse --abbrev-ref HEAD", shell=True).rstrip()

    if branch == "master" or branch == "5X_STABLE":
        return default_branch
    return branch


def render_template(template_filename, context):
    """Render pipeline template yaml"""
    return TEMPLATE_ENVIRONMENT.get_template(template_filename).render(context)

def validate_pipeline_release_jobs(raw_pipeline_yml):
    """Make sure all jobs in specified pipeline that don't block release are accounted
    for (they should belong to JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE, defined above)"""
    print "======================================================================"
    print "Validate Pipeline Release Jobs"
    print "----------------------------------------------------------------------"

    # ignore concourse v2.x variable interpolation
    pipeline_yml_cleaned = re.sub('{{', '', re.sub('}}', '', raw_pipeline_yml))
    pipeline = yaml.load(pipeline_yml_cleaned)

    jobs_raw = pipeline['jobs']
    all_job_names = [job['name'] for job in jobs_raw]

    rc_name = 'gate_release_candidate_start'
    release_candidate_job = [j for j in jobs_raw if j['name'] == rc_name][0]

    release_blocking_jobs = release_candidate_job['plan'][0]['aggregate'][0]['passed']

    non_release_blocking_jobs = [j for j in all_job_names if j not in release_blocking_jobs]

    unaccounted_for_jobs = \
        [j for j in non_release_blocking_jobs if j not in JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE]

    if unaccounted_for_jobs:
        print "Please add the following jobs as a Release_Candidate dependency or ignore them"
        print "by adding them to JOBS_THAT_SHOULD_NOT_BLOCK_RELEASE in "+ __file__
        print unaccounted_for_jobs
        return False

    print "Pipeline validated: all jobs accounted for"
    return True

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
        'pipeline_type': args.pipeline_type,
        'test_trigger': test_trigger
    }

    pipeline_yml = render_template(args.template_filename, context)
    if args.pipeline_type == 'prod':
        validated = validate_pipeline_release_jobs(pipeline_yml)
        if not validated:
            print "Refusing to update the pipeline file"
            return False

    with open(args.output_filepath, 'w') as output:
        header = render_template('pipeline_header.yml', context)
        output.write(header)
        output.write(pipeline_yml)

    return True

def how_to_use_generated_pipeline(args):
    """Generate a message for user explaining how to set the newly generated pipeline."""
    secrets_path = os.path.expanduser('~/workspace/gp-continuous-integration/secrets')
    msg = '\n'
    msg += '======================================================================\n'
    msg += '  Generate Pipeline type: .. : %s\n' % args.pipeline_type
    msg += '  Pipeline file ............ : %s\n' % args.output_filepath
    msg += '  Template file ............ : %s\n' % args.template_filename
    msg += '  OS Types ................. : %s\n' % args.os_types
    msg += '  Test sections ............ : %s\n' % args.test_sections
    msg += '  test_trigger ............. : %s\n' % args.test_trigger_false
    msg += '======================================================================\n\n'
    if args.pipeline_type == 'prod':
        msg += 'NOTE: You can set the production pipelines with the following:\n\n'
        msg += 'fly -t gpdb-prod \\\n'
        msg += '    set-pipeline \\\n'
        msg += '    -p gpdb_master \\\n'
        msg += '    -c %s \\\n' % args.output_filepath
        msg += '    -l %s/gpdb_common-ci-secrets.yml \\\n' % secrets_path
        msg += '    -l %s/gpdb_master-ci-secrets.prod.yml \\\n' % secrets_path
        msg += '    -v pipeline-name=gpdb_master\n\n'

        msg += 'fly -t gpdb-prod \\\n'
        msg += '    set-pipeline \\\n'
        msg += '    -p gpdb_master_without_asserts \\\n'
        msg += '    -c %s \\\n' % args.output_filepath
        msg += '    -l %s/gpdb_common-ci-secrets.yml \\\n' % secrets_path
        msg += '    -l %s/gpdb_master_without_asserts-ci-secrets.prod.yml \\\n' % secrets_path
        msg += '    -v pipeline-name=gpdb_master_without_asserts\n'
    else:
        pipeline_name = os.path.basename(args.output_filepath).rsplit('.', 1)[0]
        msg += 'NOTE: You can set the developer pipeline with the following:\n\n'
        msg += 'fly -t gpdb-dev \\\n'
        msg += '    set-pipeline \\\n'
        msg += '    -p %s \\\n' % pipeline_name
        msg += '    -c %s \\\n' % args.output_filepath
        msg += '    -l %s/gpdb_common-ci-secrets.yml \\\n' % secrets_path
        msg += '    -l %s/gpdb_master-ci-secrets.dev.yml \\\n' % secrets_path
        msg += '    -l %s/ccp_ci_secrets_gpdb-dev.yml \\\n' % secrets_path
        msg += '    -v gpdb-git-remote=%s \\\n' % suggested_git_remote()
        msg += '    -v gpdb-git-branch=%s \\\n' % suggested_git_branch()
        msg += '    -v pipeline-name=%s \n' % pipeline_name

    return msg


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

    default_output_filename = "gpdb_master-generated.yml"
    parser.add_argument(
        '-o',
        '--output',
        action='store',
        dest='output_filepath',
        default=os.path.join(PIPELINES_DIR, default_output_filename),
        help='Output filepath'
    )

    parser.add_argument(
        '-O',
        '--os_types',
        action='store',
        dest='os_types',
        default=['centos6'],
        choices=['centos6', 'centos7', 'sles', 'win', 'ubuntu16'],
        nargs='+',
        help='List of OS values to support'
    )

    parser.add_argument(
        '-t',
        '--pipeline_type',
        action='store',
        dest='pipeline_type',
        default='dev',
        help='Pipeline type (production="prod")'
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
            'Extensions',
            'Gpperfmon'
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
        help='Developer userid to use for pipeline file name.'
    )

    args = parser.parse_args()

    if args.pipeline_type == 'prod':
        args.os_types = ['centos6', 'centos7', 'sles', 'win', 'ubuntu16']
        args.test_sections = [
            'ICW',
            'Replication',
            'ResourceGroups',
            'Interconnect',
            'CLI',
            'UD',
            'Extensions',
            'Gpperfmon'
        ]

    # if generating a dev pipeline but didn't specify an output,
    # don't overwrite the master pipeline
    if (args.pipeline_type != 'prod' and
            os.path.basename(args.output_filepath) == default_output_filename):
        default_dev_output_filename = 'gpdb-' + args.pipeline_type + '-' + args.user + '.yml'
        args.output_filepath = os.path.join(PIPELINES_DIR, default_dev_output_filename)

    pipeline_created = create_pipeline(args)

    if not pipeline_created:
        exit(1)

    print how_to_use_generated_pipeline(args)

if __name__ == "__main__":
    main()
