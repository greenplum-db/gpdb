#!/bin/bash
#
# This is a bash script (since /bin/bash is guaranteed) that sets up the environment
# needed to execute a python script.  It uses the path to itself as the basis from
# which it determines GPHOME.

# Get the absolute path to this script
absPath=`cd $(dirname $0); pwd`

# absPath is $GPHOME/bin/lib, so backup two directories to find $GPHOME
GPHOME=$(dirname $(dirname $absPath))
export GPHOME

# get the PATH setup
PATH=$GPHOME/bin:$GPHOME/ext/python/bin:$PATH
export PATH

# Set PYTHONPATH and HOME
PYTHONPATH=$GPHOME/lib/python
export PYTHONPATH
PYTHONHOME=$GPHOME/ext/python
export PYTHONHOME

# Add in library paths apropriate for this system
case `uname` in
  Darwin) 
     if [ $DYLD_LIBRARY_PATH ]
     then DYLD_LIBRARY_PATH=$GPHOME/lib:$GPHOME/ext/python/lib:$DYLD_LIBRARY_PATH
     else DYLD_LIBRARY_PATH=$GPHOME/lib:$GPHOME/ext/python/lib
     fi
     export DYLD_LIBRARY_PATH
     ;;
  *) 
     if [ $LD_LIBRARY_PATH ]
     then LD_LIBRARY_PATH=$GPHOME/lib:$GPHOME/ext/python/lib:$LD_LIBRARY_PATH
     else LD_LIBRARY_PATH=$GPHOME/lib:$GPHOME/ext/python/lib
     fi
     export LD_LIBRARY_PATH
     ;;
esac


# execute whatever was given to us
if [ "$#" -gt 0 ]
then eval $*
else env
fi
