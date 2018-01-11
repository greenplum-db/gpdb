import os
import subprocess
import sys

class GpdbBuildBase:
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
	
    def install_dependency(self, dependency_name, untar_dir="/usr/local"):
        subprocess.call("mkdir -p {0}".format(untar_dir), shell=True)
        subprocess.call("tar -xzf {0}/*.tar.gz -C {1}".format(dependency_name, untar_dir), shell=True)
        return subprocess.call(["ldconfig", os.path.join(untar_dir, "lib")])
    
    def configure(self):
        return subprocess.call(["./configure",
                                "--enable-mapreduce",
                                "--with-perl",
                                "--with-libxml",
                                "--with-python",
                                "--disable-gpfdist",
                                "--prefix=/usr/local/gpdb"], cwd="gpdb_src")
    
