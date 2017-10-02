1. ORCA require [CMake](https://cmake.org), make sure you have it installed.
   Installation instructions vary, please check the CMake website.
   
1. Install needed python modules

  Add the following Python modules (2.7 & 2.6 are supported)

  * psutil
  * lockfile (>= 0.9.1)
  * paramiko
  * setuptools

  If necessary, upgrade modules using "pip install --upgrade".
  pip should be at least version 7.x.x.

2. Verify that you can ssh to your machine name without a password
```
ssh <hostname of your machine>  # e.g., ssh briarwood
```

