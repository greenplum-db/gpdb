#!/usr/bin/python2

import argparse
import os
import subprocess
import sys

BASE_DIR = os.getcwd()
def build_install_orca(prefix, path):
    # by default conan will install the files under /usr/local
    return subprocess.call(["./configure --prefix={0} && make && make install_local".format(prefix)],
                           shell=True,
                           cwd="gpdb_src/depends",
                           env={"PATH": path,
                                "CONAN_CMAKE_GENERATOR": "Ninja"})


def package_orca(orca_install_dir_prefix):
    return subprocess.call(["tar -czf {0}/bin_orca/bin_orca_conan.tar.gz ./include ./lib".format(BASE_DIR)],
                           shell=True, cwd=orca_install_dir_prefix)


def main():
    parser = argparse.ArgumentParser(description='Main driver to build and install ORCA using conan')
    parser.add_argument('--prefix', help='Configure prefix path', type=str, default='/tmp/')
    required_arguments = parser.add_argument_group('required arguments')
    required_arguments.add_argument('--cmakepath', help='cmake bin directory path', type=str, default='/root/cmake-3.8.2-Linux-x86_64/bin/')
    args = parser.parse_args()

    path = os.environ['PATH']
    if args.cmakepath:
        path = args.cmakepath + ':' + path

    status = build_install_orca(args.prefix, path)
    if status:
        return status

    status = package_orca(args.prefix)
    if status:
        return status
if __name__ == "__main__":
    sys.exit(main())
