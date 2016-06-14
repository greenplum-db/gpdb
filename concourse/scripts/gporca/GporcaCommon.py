import os
import subprocess
import sys

class GporcaCommon:
    def __init__(self):
        pass

    def num_cpus(self):
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
	
    def yum_install(self, pkg):
        return subprocess.call(["yum", "install", "-y", pkg])
    
    def install_system_deps(self):
        status = subprocess.call(["yum", "--exclude=systemtap",
                                  "groupinstall", "-y", "Development tools"])
        if status:
            return status
    
        status = subprocess.call(["yum", "install", "-y",
                                  "sudo",
                                  "passwd",
                                  "openssh-server",
                                  "ed",
                                  "readline-devel",
                                  "zlib-devel",
                                  "curl-devel",
                                  "bzip2-devel",
                                  "python-devel",
                                  "apr-devel",
                                  "libevent-devel",
                                  "openssl-libs",
                                  "openssl-devel",
                                  "libyaml",
                                  "libyaml-devel",
                                  "epel-release",
                                  "htop",
                                  "perl-Env",
                                  "perl-ExtUtils-Embed",
                                  "libxml2-devel",
                                  "libxslt-devel",
                                  "libffi-devel"])
        if status:
            return status
    
        status = subprocess.call(["curl", "https://bootstrap.pypa.io/get-pip.py", "-o", "get-pip.py"])
        if status:
            return status
    
        status = subprocess.call(["python", "get-pip.py"])
        if status:
            return status
    
        status = subprocess.call(["pip", "install", "psutil", "lockfile", "paramiko", "setuptools",
                                  "epydoc"])
        return status
    
    def install_dependency(self, dependency_name):
        return subprocess.call(
            ["tar",
             "-xzf",
             dependency_name + "/" + dependency_name + ".tar.gz",
             "-C",
             "/usr/local"])
    
    def configure(self):
        return subprocess.call(["./configure",
                                "--enable-orca",
                                "--enable-mapreduce",
                                "--with-perl",
                                "--with-libxml",
                                "--with-python",
                                "--disable-gpfdist",
                                "--prefix=/usr/local/gpdb"], cwd="gpdb_src")
    
