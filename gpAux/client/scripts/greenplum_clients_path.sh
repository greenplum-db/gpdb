GPHOME_CLIENTS=`pwd`
PATH=${GPHOME_CLIENTS}/bin:${GPHOME_CLIENTS}/ext/python/bin:${PATH}
PYTHONPATH=${GPHOME_CLIENTS}/bin/ext:${PYTHONPATH}

# Export GPHOME_LOADERS for GPDB5 compatible
GPHOME_LOADERS=${GPHOME_CLIENTS}
export GPHOME_CLIENTS
export GPHOME_LOADERS
export PATH
export PYTHONPATH

# Mac OSX uses a different library path variable
if [ xDarwin = x`uname -s` ]; then
  DYLD_LIBRARY_PATH=${GPHOME_CLIENTS}/lib:${GPHOME_CLIENTS}/ext/python/lib:${DYLD_LIBRARY_PATH}
  export DYLD_LIBRARY_PATH
else
  LD_LIBRARY_PATH=${GPHOME_CLIENTS}/lib:${GPHOME_CLIENTS}/ext/python/lib:${LD_LIBRARY_PATH}
  export LD_LIBRARY_PATH
fi

# AIX uses yet another library path variable
# Also, Python on AIX requires special copies of some libraries.  Hence, lib/pware.
if [ xAIX = x`uname -s` ]; then
  LIBPATH=${GPHOME_CLIENTS}/lib/pware:${GPHOME_CLIENTS}/lib:${GPHOME_CLIENTS}/ext/python/lib64:/usr/lib/threads:${LIBPATH}
  export LIBPATH
  GP_LIBPATH_FOR_PYTHON=${GPHOME_CLIENTS}/lib/pware
  export GP_LIBPATH_FOR_PYTHON
fi

if [ "$1" != "-q" ]; then
  type python >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "Warning: Python not found.  Python-2.5.1 or better is required to run gpload."
  fi
fi
