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

"""Generate pipeline (default: gpdb_6X_STABLE-generated.yml) from template (default:
templates/gpdb-tpl.yml).

Python module requirements:
  - jinja2 (install through pip or easy_install)
"""

from __future__ import print_function

import argparse
import datetime
import getpass
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

BASE_BRANCH = "6X_STABLE"  # when branching gpdb update to 7X_STABLE, 6X_STABLE, etc.

CI_VARS_PATH = os.path.join(os.getcwd(), '..', 'vars')

default_os_type = 'rocky8'

def suggested_git_remote():
    """Try to guess the current git remote"""
    default_remote = "<https://github.com/<github-user>/gpdb>"
    staging_remote = "git@github.com:pivotal/gp-gpdb-staging"
    remote = subprocess.check_output(["git", "ls-remote", "--get-url"]).decode('utf-8').rstrip()

    if "greenplum-db/gpdb" in remote:
        return default_remote

    if "pivotal/gp-gpdb-staging" in remote:
        return staging_remote

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


def create_pipeline(args, git_remote, git_branch):
    """Generate OS specific pipeline sections"""

    variables_type = args.pipeline_target
    os_username = {
        "centos6" : "centos",
        "centos7" : "centos",
        "rhel8" : "rhel",
        "ubuntu20.04" : "ubuntu",
        "rocky8" : "rocky",
        "rocky9" : "rocky",
        "oel8" : "oel",
        "oel7" : "oel",
        "rhel9" : "rhel",
        "oel9" : "oel"
    }
    test_os = {
        "centos6" : "centos",
        "centos7" : "centos",
        "rhel8" : "centos",
        "ubuntu20.04" : "ubuntu",
        "rocky8" : "centos",
        "rocky9" : "centos",
        "oel8" : "centos",
        "oel7" : "centos",
        "rhel9" : "centos",
        "oel9": "centos"
    }
    dist = {
        "centos6" : "rhel6",
        "centos7" : "rhel7",
        "rhel8" : "rhel8",
        "ubuntu20.04" : "ubuntu20.04",
        "rocky8" : "rhel8",
        "rocky9" : "rhel9",
        "oel8" : "rhel8",
        "oel7" : "oel7",
        "rhel9" : "rhel9",
        "oel9": "rhel9"
    }
    rpm_platform = {
        "centos6" : "rhel6",
        "centos7" : "rhel7",
        "rhel8" : "rhel8",
        "ubuntu20.04" : "ubuntu20.04",
        "rocky8" : "rocky8",
        "rocky9" : "rocky9",
        "oel8" : "oel8",
        "oel7" : "oel7",
        "rhel9" : "rhel9",
        "oel9": "oel9"
    }
    compile_platform = {
        "centos6": "centos6",
        "centos7": "centos7",
        "rhel8": "rocky8",
        "ubuntu20.04": "ubuntu20.04",
        "rocky8": "rocky8",
        "rocky9": "rocky9",
        "oel8": "rocky8",
        "oel7": "oel7",
        "rhel9" : "rocky9",
        "oel9": "rocky9"
    }
    context = {
        'template_filename': args.template_filename,
        'generator_filename': os.path.basename(__file__),
        'timestamp': datetime.datetime.now(),
        'os_type': args.os_type,
        'default_os_type': default_os_type,
        'os_username': os_username[args.os_type],
        'test_os': test_os[args.os_type],
        'dist': dist[args.os_type],
        'rpm_platform': rpm_platform[args.os_type],
        'compile_platform': compile_platform[args.os_type],
        'pipeline_target': args.pipeline_target,
        'test_sections': args.test_sections,
        'use_ICW_workers': args.use_ICW_workers,
        'build_test_rc_rpm': args.build_test_rc_rpm,
        'directed_release': args.directed_release,
        'git_username': git_remote.split('/')[-2],
        'git_branch': git_branch,
        'variables_type': variables_type
    }

    pipeline_yml = render_template(args.template_filename, context)

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

    return '''fly -t {target} \
set-pipeline \
-p {name} \
-c {output_path} \
{variables} \
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
  OS Type .................. : %s
  Test sections ............ : %s
  use_ICW_workers .......... : %s
  build_test_rc_rpm ........ : %s
  directed_release ......... : %s
======================================================================
''' % (args.pipeline_target,
       args.output_filepath,
       args.template_filename,
       args.os_type,
       args.test_sections,
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
                           suggested_git_remote(), git_branch))
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
        choices=['centos6', 'centos7', 'rhel8','ubuntu20.04', 'rocky8', 'oel8', 'oel7', 'rhel9', "oel9", 'rocky9'],
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
        '-a',
        '--test_sections',
        action='store',
        dest='test_sections',
        choices=[
            'icw',
            'cli',
            'aa',
            'release'
        ],
        default=[],
        nargs='+',
        help='Select tests sections to run, release section should be specified with {icw,cli,aa}, and will be ignored if os_type is rhel8 or oel8'
    )

    parser.add_argument(
        '-u',
        '--user',
        action='store',
        dest='user',
        default=getpass.getuser(),
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
    if (args.user != getpass.getuser() and output_path_is_set):
        print("You can only use one of --output or --user.")
        exit(1)

    # use_ICW_workers adds tags to the specified concourse definitions which
    # correspond to dedicated concourse workers to increase performance.
    if args.pipeline_target in ['prod', 'dev', 'cm']:
        args.use_ICW_workers = True

    if args.pipeline_target in ['prod'] :
        args.test_sections = [
            'icw',
            'cli',
            'aa',
            'release'
        ]

    if args.directed_release:
        args.test_sections = [
            'icw',
            'cli',
            'aa',
            'release'
        ]

    git_remote = suggested_git_remote()
    git_branch = suggested_git_branch()

    # if generating a dev pipeline but didn't specify an output,
    # don't overwrite the 6X_STABLE pipeline
    if args.pipeline_target != 'prod' and not output_path_is_set:
        pipeline_file_suffix = suggested_git_branch()
        if args.user != getpass.getuser():
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
