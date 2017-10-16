#!/usr/bin/python2

import subprocess
import sys
import os
import argparse

def add_orca_conan_remote(bintrayRemote, bintrayRemoteURL):
    return subprocess.call(["conan", "remote", "add", bintrayRemote, bintrayRemoteURL])

def build_install_orca(build_type, destdir=None, env_additions=""):
    # by default conan will install the files under /usr/local
    environ = "env PATH=/root/cmake-3.8.2-Linux-x86_64/bin/:$PATH CONAN_CMAKE_GENERATOR=Ninja "+ env_additions + " "
    if destdir:
	environ += "DESTDIR={0}".format(destdir)

    return subprocess.call(["{0} conan install -s build_type={1} --build".format(environ, build_type)],
				    shell=True,
				    cwd="gpdb_src/depends")

def package_orca(base_dir):
    return subprocess.call(["tar -czf {0}/bin_orca/bin_orca_conan.tar.gz ./include ./lib".format(base_dir)], shell=True, cwd="bin_orca/usr/local")

def main():
    parser = argparse.ArgumentParser(description='Main driver to build and install ORCA using conan')
    parser.add_argument('--env_additions', help='Additional env settings', type=str, default="")
    parser.add_argument('--build_type', help='Build type, i.e Release or Debug', type=str, default='Release')
    required_arguments = parser.add_argument_group('required arguments')
    required_arguments.add_argument('--bintrayRemote', help='Name of conan remote refering to bintray', type=str)
    required_arguments.add_argument('--bintrayRemoteURL', help='Conan remote URL pointing to bintray repo', type=str)
    args = parser.parse_args()

    status = add_orca_conan_remote(args.bintrayRemote, args.bintrayRemoteURL)
    if status:
	return status

    status = build_install_orca(args.build_type, os.path.join(os.getcwd(), 'bin_orca'),
                                env_additions=args.env_additions)
    if status:
	return status

    status = package_orca(os.getcwd())
    if status:
	return status

if __name__ == "__main__":
    sys.exit(main())
