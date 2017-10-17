#!/usr/bin/python2

import optparse
import subprocess
import sys

from builds.GpBuild import GpBuild

CODEGEN_MODE = 'codegen'
ORCA_CODEGEN_DEFAULT_MODE = "orca_codegen"
ORCA_MODE = 'orca'
PLANNER_MODE = 'planner'


def make(num_cpus):
    return subprocess.call(["make",
                            "-j" + str(num_cpus),
                            "-l" + str(2 * num_cpus),
                            ],
                           cwd="gpdb_src")


def install(output_dir):
    subprocess.call("make install", cwd="gpdb_src", shell=True)
    subprocess.call("mkdir -p " + output_dir, shell=True)
    return subprocess.call("cp -r /usr/local/gpdb/* " + output_dir, shell=True)


def unittest():
    return subprocess.call("make -s unittest-check", cwd="gpdb_src/src/backend", shell=True)


def print_compiler_version():
    subprocess.call(["g++", "--version"])
    subprocess.call(["gcc", "--version"])


def main():
    parser = optparse.OptionParser()
    parser.add_option("--build_type", dest="build_type", default="RELEASE")
    parser.add_option("--mode", choices=[ORCA_MODE, CODEGEN_MODE, ORCA_CODEGEN_DEFAULT_MODE, PLANNER_MODE], default=ORCA_CODEGEN_DEFAULT_MODE)
    parser.add_option("--compiler", dest="compiler")
    parser.add_option("--cxxflags", dest="cxxflags")
    parser.add_option("--output_dir", dest="output_dir", default="install")
    (options, args) = parser.parse_args()
    ci_common = GpBuild(ORCA_CODEGEN_DEFAULT_MODE)
    if options.mode == ORCA_MODE:
        ci_common = GpBuild(options.mode)
    elif options.mode == PLANNER_MODE:
        ci_common = GpBuild(options.mode)

    for dependency in args:
        status = ci_common.install_dependency(dependency)
        if status:
            return status
    print_compiler_version()
    status = ci_common.configure()
    if status:
        return status
    status = make(ci_common.num_cpus())
    if status:
        return status
    status = unittest()
    if status:
        return status
    status = install(options.output_dir)
    if status:
        return status
    return 0


if __name__ == "__main__":
    sys.exit(main())
