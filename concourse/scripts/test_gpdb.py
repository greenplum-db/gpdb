#!/usr/bin/python2

import optparse
import os
import shutil
import stat
import subprocess
import sys

from builds.GpBuild import GpBuild
from subprocess import check_output

def install_gpdb(dependency_name):
    status = subprocess.call("mkdir -p /usr/local/gpdb", shell=True)
    if status:
        return status
    status = subprocess.call(
        "tar -xzf " + dependency_name + "/*.tar.gz -C /usr/local/gpdb",
        shell=True)
    return status

def create_gpadmin_user():
    status = subprocess.call("gpdb_src/concourse/scripts/setup_gpadmin_user.bash")
    os.chmod('/bin/ping', os.stat('/bin/ping').st_mode | stat.S_ISUID)
    if status:
        return status


def copy_output():
    diff_files = check_output("find gpdb_src/ -name regression.diffs", shell=True).splitlines()
    for diff_file in diff_files:
        print(  "======================================================================\n" +
                "DIFF FILE: " + diff_file+"\n" +
                "----------------------------------------------------------------------")
        with open(diff_file, 'r') as fin:
            print fin.read()

def main():
    parser = optparse.OptionParser()
    parser.add_option("--build_type", dest="build_type", default="RELEASE")
    parser.add_option("--mode",  choices=['orca', 'codegen', 'orca_codegen', 'planner'])
    parser.add_option("--compiler", dest="compiler")
    parser.add_option("--cxxflags", dest="cxxflags")
    parser.add_option("--output_dir", dest="output_dir", default="install")
    parser.add_option("--gpdb_name", dest="gpdb_name")
    (options, args) = parser.parse_args()
    if options.mode == 'orca':
        ciCommon = GpBuild(options.mode)
    elif options.mode == 'planner':
        ciCommon = GpBuild(options.mode)

    for dependency in args:
        status = ciCommon.install_dependency(dependency)
        if status:
            return status
    status = install_gpdb(options.gpdb_name)
    if status:
        return status
    status = ciCommon.configure()
    if status:
        return status
    status = create_gpadmin_user()
    if status:
        return status
    if os.getenv("TEST_SUITE", "icg") == 'icw':
      status = ciCommon.install_check('world')
    else:
      status = ciCommon.install_check()
    if status:
        copy_output()
    return status

if __name__ == "__main__":
    sys.exit(main())
