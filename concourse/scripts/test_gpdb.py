#!/usr/bin/python2

import optparse
import subprocess
import sys
import shutil
from builds import GpBuild, GpcodegenBuild, GporcacodegenBuild

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
    if status:
        return status


def copy_output():
    shutil.copyfile("gpdb_src/src/test/regress/regression.diffs", "icg_output/regression.diffs")
    shutil.copyfile("gpdb_src/src/test/regress/regression.out", "icg_output/regression.out")

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
    elif options.mode == 'codegen':
        ciCommon = GpcodegenBuild()
    elif options.mode == 'orca_codegen':
        ciCommon = GporcacodegenBuild()

    status = ciCommon.install_system_deps()
    if status:
        return status
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
    status = ciCommon.icg()
    if status:
        copy_output()
    return status

if __name__ == "__main__":
    sys.exit(main())
