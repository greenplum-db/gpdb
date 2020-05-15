#!/usr/bin/python

import optparse
import os
import os.path
import subprocess
import sys
import tarfile
import urllib2
import hashlib

XERCES_SOURCE_URL = "http://archive.apache.org/dist/xerces/c/3/sources/xerces-c-3.1.2.tar.gz"
XERCES_SOURCE_DIR = "xerces-c-3.1.2"

def num_cpus():
    # Use multiprocessing module, available in Python 2.6+
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except (ImportError, NotImplementedError):
        pass

    # Get POSIX system config value for number of processors.
    posix_num_cpus = os.sysconf("SC_NPROCESSORS_ONLN")
    if posix_num_cpus != -1:
        return posix_num_cpus

    # Guess
    return 2

def get_xerces_source():
    remote_src = urllib2.urlopen(XERCES_SOURCE_URL)
    local_src = open("xerces_src.tar.gz", "w")
    local_src.write(remote_src.read())
    local_src.close()
    file_hash = hashlib.sha256(open('xerces_src.tar.gz','rb').read()).hexdigest()
    actual_hash = ""
    with open('xerces_patch/concourse/xerces-c/xerces-c-3.1.2.tar.gz.sha256', 'r') as f:
        actual_hash = f.read().strip()
    if file_hash != actual_hash:
        return 1
    tarball = tarfile.open("xerces_src.tar.gz", "r:gz")
    for item in tarball:
        tarball.extract(item, ".")
    tarball.close()
    return 0

def configure(cxx_compiler, cxxflags, cflags, output_dir):
    os.mkdir("build")
    environment = os.environ.copy()
    if cxx_compiler:
        environment["CXX"] = cxx_compiler
    if cxxflags:
        environment["CXXFLAGS"] = cxxflags
    if cflags:
        environment["CFLAGS"] = cflags
    return subprocess.call(
        [os.path.abspath(XERCES_SOURCE_DIR + "/configure"), "--prefix=" + os.path.abspath(output_dir)],
        env = environment,
        cwd = "build")

def make():
    return subprocess.call(["make", "-j" + str(num_cpus())], cwd="build")

def install():
    return subprocess.call(["make", "install"], cwd="build")

def main():
    parser = optparse.OptionParser()
    parser.add_option("--compiler", dest="compiler")
    parser.add_option("--cxxflags", dest="cxxflags")
    parser.add_option("--cflags", dest="cflags")
    parser.add_option("--output_dir", dest="output_dir", default="install")
    (options, args) = parser.parse_args()
    if len(args) > 0:
        print "Unknown arguments"
        return 1
    status = get_xerces_source()
    if status:
        return status
    status = configure(options.compiler, options.cxxflags, options.cflags, options.output_dir)
    if status:
        return status
    status = make()
    if status:
        return status
    status = install()
    if status:
        return status
    return 0

if __name__ == "__main__":
    sys.exit(main())
