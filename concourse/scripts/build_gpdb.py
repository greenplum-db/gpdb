#!/usr/bin/python2

import optparse
import subprocess
import sys
from builds import GpBuild, GpcodegenBuild, GporcacodegenBuild

def make(num_cpus):
    return subprocess.call("make -j %d" % (num_cpus), cwd="gpdb_src", shell=True)

def install(output_dir):
    subprocess.call("make install", cwd="gpdb_src", shell=True)
    subprocess.call("mkdir -p " + output_dir, shell=True)
    return subprocess.call("cp -r /usr/local/gpdb/* " + output_dir, shell=True)

def unittest():
        return subprocess.call("make -s unittest-check", cwd="gpdb_src/src/backend", shell=True)

def main():
    parser = optparse.OptionParser()
    parser.add_option("--build_type", dest="build_type", default="RELEASE")
    parser.add_option("--mode", choices=['orca', 'codegen', 'orca_codegen', 'planner'], default="orca_codegen")
    parser.add_option("--compiler", dest="compiler")
    parser.add_option("--cxxflags", dest="cxxflags")
    parser.add_option("--output_dir", dest="output_dir", default="install")
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
    status = ciCommon.configure()
    if status:
        return status
    status = make(ciCommon.num_cpus())
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
