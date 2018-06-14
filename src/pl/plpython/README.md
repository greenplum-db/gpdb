# Python3 Support
## How to build: 
```
# Install Python3 (take centos7 as example)
yum install -y python36-devel

# Configure build environment to install at /usr/local/gpdb
PYTHON=/usr/bin/python3.6 ./configure --with-perl --with-python --with-libxml --prefix=/usr/local/gpdb

# Compile and install
make -j8
make -j8 install
```


## How to use:
```
# Ensure your environment include the SAME python version as the build environment, in our example is Python3.6.
# If you install Python in a non-system folder, you also need to add $PYTHONHOME/lib into your shared library path.
yum install -y python36-devel

# Install Python3 packages:
wget https://bootstrap.pypa.io/get-pip.py
python3 get-pip.py
pip3 install numpy

# Test Python3 support in GPDB 
CREATE LANGUAGE plpython3u;
CREATE FUNCTION import_succeed() returns text AS $$
  import sys
  import numpy
  return "succeeded, as expected"
$$ LANGUAGE plpython3u;
SELECT import_succeed();

CREATE TYPE named_value AS (
         name  text,
         value  integer);

CREATE OR REPLACE FUNCTION make_pair_sets (name text)
RETURNS SETOF named_value AS $$
  import numpy as np
  return ((name, i) for i in np.arange(1))
$$ LANGUAGE plpython3u;

SELECT * from make_pair_sets('test');
```

