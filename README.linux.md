1. ORCA requires [CMake](https://cmake.org); make sure you have it installed.
   Installation instructions vary, please check the CMake website.
   
1. Install needed python modules

    Add the following Python modules (2.7 & 2.6 are supported)

    * psutil
    * lockfile (>= 0.9.1)
    * paramiko
    * setuptools

    If necessary, upgrade modules using "pip install --upgrade".
    pip should be at least version 7.x.x.

1. Verify that you can ssh to your machine name without a password

    ```
    ssh <hostname of your machine>  # e.g., ssh briarwood
    ```

1. If you are using CentOS, first make sure that you add `/usr/local/lib` and
   `/usr/local/lib64` to `/etc/ld.so.conf`, then run command `ldconfig`.

    If you are using Ubuntu, confirm that `/usr/local/lib` is included via
	`/etc/ld.so.conf`, then run command `ldconfig`.
